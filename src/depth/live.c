#include "portable.h"
#include "accelerated.h"
#include <k4ainternal/depth_mcu.h>
#include <k4ainternal/image.h>
#include <k4ainternal/allocator.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#define RAW_SIZE 5310760
#define PIXELS (640 * 576)
#define RGB_WIDTH 1280
#define RGB_HEIGHT 720
static k4a_calibration_t camera_calibration;
static k4a_float3_t rays[PIXELS];
static uint16_t aligned[RGB_WIDTH * RGB_HEIGHT];
char K4A_ENV_VAR_LOG_TO_A_FILE[] = "K4A_ENABLE_LOG_TO_A_FILE";
static volatile sig_atomic_t stopping;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static uint8_t pending[RAW_SIZE];
static bool have_frame;
static unsigned long received, dropped;
static uint64_t receipt_ns;

static void stop(int sig) { (void)sig; stopping=1; }

static bool load_camera_calibration(depthmcu_t device, const char *directory)
{
    char *json=calloc(1,2000000);
    size_t size=0;
    if(!json) return false;
    bool ok=depthmcu_get_extrinsic_calibration(device,json,1999999,&size)==K4A_RESULT_SUCCEEDED;
    if(ok)
    {
        char path[4096];
        snprintf(path,sizeof(path),"%s/camera-calibration.json",directory);
        FILE *file=fopen(path,"wb");
        if(file) { fwrite(json,1,size,file); fclose(file); }
        ok=k4a_calibration_get_from_raw(json,size+1,K4A_DEPTH_MODE_NFOV_UNBINNED,
                                       K4A_COLOR_RESOLUTION_720P,&camera_calibration)==K4A_RESULT_SUCCEEDED;
    }
    free(json);
    if(!ok) return false;
    for(int y=0;y<576;y++) for(int x=0;x<640;x++)
    {
        k4a_float2_t pixel={{(float)x,(float)y}};
        int valid=0;
        k4a_float3_t ray={{0,0,0}};
        if(k4a_calibration_2d_to_3d(&camera_calibration,&pixel,1.0f,K4A_CALIBRATION_TYPE_DEPTH,
                                  K4A_CALIBRATION_TYPE_DEPTH,&ray,&valid)==K4A_RESULT_SUCCEEDED && valid)
            rays[y*640+x]=ray;
    }
    fprintf(stderr,"Using this device's camera intrinsics/extrinsics for 1280x720 RGB projection\n");
    return true;
}

static void project_depth(const uint16_t *depth)
{
    memset(aligned,0,sizeof(aligned));
    for(int i=0;i<PIXELS;i++)
    {
        if(!depth[i] || rays[i].xyz.z<=0) continue;
        k4a_float3_t point={{rays[i].xyz.x*depth[i],rays[i].xyz.y*depth[i],(float)depth[i]}};
        k4a_float2_t pixel;
        int valid=0;
        if(k4a_calibration_3d_to_2d(&camera_calibration,&point,K4A_CALIBRATION_TYPE_DEPTH,
                                  K4A_CALIBRATION_TYPE_COLOR,&pixel,&valid)!=K4A_RESULT_SUCCEEDED || !valid) continue;
        if(!isfinite(pixel.xy.x) || !isfinite(pixel.xy.y) || pixel.xy.x<0 || pixel.xy.x>=RGB_WIDTH ||
           pixel.xy.y<0 || pixel.xy.y>=RGB_HEIGHT) continue;
        int x=(int)pixel.xy.x,y=(int)pixel.xy.y;
        /* Small point footprints fill upsampling gaps; keep nearest depth on collisions.
         * This is an approximate rasterization, not the SDK's triangle/occlusion pipeline. */
        for(int dy=-1;dy<=1;dy++) for(int dx=-1;dx<=1;dx++)
        {
            int xx=x+dx,yy=y+dy;
            if(xx<0 || xx>=RGB_WIDTH || yy<0 || yy>=RGB_HEIGHT) continue;
            uint16_t *value=&aligned[yy*RGB_WIDTH+xx];
            if(!*value || depth[i]<*value) *value=depth[i];
        }
    }
}

static void frame(k4a_result_t result, k4a_image_t image, void *context)
{
    (void)context;
    if (result != K4A_RESULT_SUCCEEDED || !image || image_get_size(image) != RAW_SIZE) return;
    pthread_mutex_lock(&lock);
    if (have_frame) dropped++;
    memcpy(pending,image_get_buffer(image),RAW_SIZE);
    receipt_ns=openk4a_now_fine_nsec();
    received++;
    have_frame=true;
    pthread_cond_signal(&ready);
    pthread_mutex_unlock(&lock);
}

int main(int argc,char **argv)
{
    if(argc!=2) { fprintf(stderr,"Usage: live_depth existing-output-directory\n"); return 1; }
    char temp[4096],dest[4096];
    if (snprintf(temp,sizeof(temp),"%s/frame.tmp",argv[1]) >= (int)sizeof(temp) ||
        snprintf(dest,sizeof(dest),"%s/frame.bin",argv[1]) >= (int)sizeof(dest)) return 1;
    char source[512];
    openk4a_depth_model_t *model=openk4a_depth_model_open(K4A_DEPTH_MODE_NFOV_UNBINNED,NULL,0,NULL,source,sizeof(source));
    if(!model) return 2;
    const char *backend=getenv("KINECT_DEPTH_BACKEND");
    accelerated_depth *decoder=accelerated_depth_create(!backend || strcmp(backend,"cpu")!=0);
    if(!decoder) { openk4a_depth_model_free(model); return 2; }
    fprintf(stderr,"Depth backend: %s\n",accelerated_depth_uses_metal(decoder)?"Metal filter + CPU reconstruction":"CPU");
    fprintf(stderr,"EXPERIMENTAL depth, calibration: %s; accuracy on this device NOT validated\n",source);
    uint8_t *raw=malloc(RAW_SIZE);
    uint16_t *depth=malloc(PIXELS*2);
    if(!raw || !depth) { free(raw); free(depth); accelerated_depth_destroy(decoder); openk4a_depth_model_free(model); return 2; }
    allocator_initialize();
    depthmcu_t device=NULL;
    int exit_code=0;
    bool configured=false;
    if(depthmcu_create(0,&device)!=K4A_RESULT_SUCCEEDED || !depthmcu_wait_is_ready(device)) { exit_code=3; goto cleanup; }
    depthmcu_depth_stop_streaming(device,true);
    if(!load_camera_calibration(device,argv[1])) { fprintf(stderr,"Camera calibration failed\n"); exit_code=7; goto cleanup; }
    configured=true;
    if(depthmcu_depth_set_capture_mode(device,K4A_DEPTH_MODE_NFOV_UNBINNED)!=K4A_RESULT_SUCCEEDED ||
       depthmcu_depth_set_fps(device,accelerated_depth_uses_metal(decoder)?K4A_FRAMES_PER_SECOND_30:K4A_FRAMES_PER_SECOND_15)!=K4A_RESULT_SUCCEEDED ||
       depthmcu_depth_start_streaming(device,frame,NULL)!=K4A_RESULT_SUCCEEDED) { exit_code=4; goto cleanup; }
    signal(SIGINT,stop);
    signal(SIGTERM,stop);
    unsigned long decoded=0;
    uint64_t last_frame=openk4a_now_fine_nsec();
    while(!stopping)
    {
        pthread_mutex_lock(&lock);
        if(!have_frame)
        {
            struct timespec deadline;
            clock_gettime(CLOCK_REALTIME,&deadline);
            deadline.tv_sec++;
            pthread_cond_timedwait(&ready,&lock,&deadline);
        }
        if(!have_frame) { pthread_mutex_unlock(&lock); if(openk4a_now_fine_nsec()-last_frame>10000000000ULL) { fprintf(stderr,"No raw frame for 10 seconds\n"); exit_code=5; break; } continue; }
        memcpy(raw,pending,RAW_SIZE);
        uint64_t host_ns=receipt_ns;
        unsigned long count=received, lost=dropped;
        have_frame=false;
        pthread_mutex_unlock(&lock);
        last_frame=openk4a_now_fine_nsec();
        openk4a_depth_times_t timing;
        if(!accelerated_depth_decode(decoder,model,raw,RAW_SIZE,depth,&timing)) { exit_code=8; break; }
        openk4a_frame_info_t info={0};
        openk4a_frame_info(raw,RAW_SIZE,&info);
        /* Native little-endian header: magic, sequence, host ns, sensor ticks, decode ns. */
        uint64_t header[5]={0x315448504544344bULL,(uint64_t)count,host_ns,info.exposure_ticks,timing.total_nsec};
        FILE *f=fopen(temp,"wb");
        if(!f) { perror(temp); exit_code=6; break; }
        bool ok=fwrite(header,sizeof(header),1,f)==1 && fwrite(depth,2,PIXELS,f)==PIXELS;
        if(fclose(f)!=0) ok=false;
        if(!ok || rename(temp,dest)!=0) { perror("publish depth frame"); exit_code=6; break; }
        project_depth(depth);
        char aligned_temp[4096],aligned_dest[4096];
        snprintf(aligned_temp,sizeof(aligned_temp),"%s/aligned.tmp",argv[1]);
        snprintf(aligned_dest,sizeof(aligned_dest),"%s/aligned.bin",argv[1]);
        f=fopen(aligned_temp,"wb");
        if(!f) { exit_code=6; break; }
        ok=fwrite(header,sizeof(header),1,f)==1 && fwrite(aligned,sizeof(aligned),1,f)==1;
        if(fclose(f)!=0) ok=false;
        if(!ok || rename(aligned_temp,aligned_dest)!=0) { exit_code=6; break; }
        decoded++;
        if(decoded%30==1) fprintf(stderr,"decoded=%lu received=%lu dropped=%lu decode_ms=%.1f\n",decoded,count,lost,(double)timing.total_nsec/1e6);
    }
cleanup:
    if(configured) depthmcu_depth_stop_streaming(device,false);
    if(device) depthmcu_destroy(device);
    free(raw); free(depth); accelerated_depth_destroy(decoder); openk4a_depth_model_free(model);
    return exit_code;
}
