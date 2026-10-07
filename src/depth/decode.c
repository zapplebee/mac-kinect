#include "portable.h"
int main(int argc, char **argv)
{
    if(argc != 3) { fprintf(stderr,"Usage: decode_depth raw-frame output-mm.bin\n"); return 1; }
    FILE *f=fopen(argv[1],"rb");
    if(!f) { perror(argv[1]); return 1; }
    const size_t size=5310760;
    uint8_t *raw=malloc(size);
    if(!raw || fread(raw,1,size,f)!=size || fgetc(f)!=EOF) { fclose(f); free(raw); return 2; }
    fclose(f);
    char source[512];
    openk4a_depth_model_t *model=openk4a_depth_model_open(K4A_DEPTH_MODE_NFOV_UNBINNED,NULL,0,NULL,source,sizeof(source));
    if(!model) { free(raw); return 3; }
    fprintf(stderr,"EXPERIMENTAL calibration source: %s\n",source);
    uint16_t *depth=calloc(640*576,sizeof(uint16_t));
    if(!depth) { openk4a_depth_model_free(model); free(raw); return 4; }
    openk4a_depth_times_t timing;
    openk4a_depth_decode_timed(model,raw,size,depth,&timing);
    size_t valid=0; unsigned min=65535,max=0;
    for(size_t i=0;i<640*576;i++) if(depth[i]) { valid++; if(depth[i]<min) min=depth[i]; if(depth[i]>max) max=depth[i]; }
    fprintf(stderr,"Valid: %zu, range: %u..%u nominal mm, decode %.1f ms\n",valid,min,max,(double)timing.total_nsec/1e6);
    f=fopen(argv[2],"wb");
    int result=0;
    if(!f) result=5;
    else { if(fwrite(depth,2,640*576,f)!=640*576) result=5; if(fclose(f)) result=5; }
    free(depth); free(raw); openk4a_depth_model_free(model);
    return result;
}
