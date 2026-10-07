#include "accelerated.h"
#include <math.h>

int main(int argc,char **argv) {
    if(argc!=2) { fprintf(stderr,"Usage: benchmark_depth complete-nfov-frame.raw\n");return 1; }
    FILE *f=fopen(argv[1],"rb");
    if(!f) { perror(argv[1]);return 1; }
    uint8_t *raw=malloc(5310760);
    if(!raw) { fclose(f);return 1; }
    bool read_ok=fread(raw,1,5310760,f)==5310760 && fgetc(f)==EOF;
    fclose(f);
    if(!read_ok) { free(raw);return 1; }
    openk4a_depth_model_t *cpu=openk4a_depth_model_open(K4A_DEPTH_MODE_NFOV_UNBINNED,NULL,0,NULL,NULL,0);
    openk4a_depth_model_t *gpu=openk4a_depth_model_open(K4A_DEPTH_MODE_NFOV_UNBINNED,NULL,0,NULL,NULL,0);
    accelerated_depth *decoder=accelerated_depth_create(true);
    uint16_t *a=calloc(640*576,2),*b=calloc(640*576,2);
    int result=0;
    if(!cpu||!gpu||!decoder||!a||!b||!accelerated_depth_uses_metal(decoder)) { result=2;goto cleanup; }
    double cpu_ns=0,gpu_ns=0;
    for(int iteration=0;iteration<11;iteration++) {
        openk4a_depth_times_t ct,gt;
        openk4a_depth_decode_timed(cpu,raw,5310760,a,&ct);
        if(!accelerated_depth_decode(decoder,gpu,raw,5310760,b,&gt)||!accelerated_depth_uses_metal(decoder)) { result=3;goto cleanup; }
        if(iteration) { cpu_ns+=(double)ct.total_nsec;gpu_ns+=(double)gt.total_nsec; }
    }
    size_t unequal=0,invalid=0,large=0; unsigned max_error=0;
    for(size_t i=0;i<640*576;i++) {
        if(a[i]!=b[i]) unequal++;
        if((a[i]==0)!=(b[i]==0)) invalid++;
        else if(a[i] && b[i]) {
            unsigned error=(unsigned)abs((int)a[i]-(int)b[i]);
            if(error>max_error) max_error=error;
            if(error>2) large++;
        }
    }
    printf("CPU mean %.3f ms, Metal hybrid mean %.3f ms, speedup %.2fx\n",cpu_ns/1e7,gpu_ns/1e7,cpu_ns/gpu_ns);
    printf("unequal=%zu invalid-mask-differences=%zu >2mm=%zu max-valid-error=%u mm\n",unequal,invalid,large,max_error);
    /* Tiny floating-point differences near confidence thresholds are expected,
       but large differences or broad mask changes are not acceptable. */
    if(invalid>36 || large>36) result=4;
cleanup:
    free(raw);free(a);free(b);accelerated_depth_destroy(decoder);
    openk4a_depth_model_free(cpu);openk4a_depth_model_free(gpu);
    return result;
}
