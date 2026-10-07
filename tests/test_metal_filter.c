#include "metal_filter.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static int check(unsigned width,unsigned height) {
    size_t n=(size_t)width*height;
    metal_filter *filter=metal_filter_create(width,height);
    if(!filter) return 77;
    float *proj=calloc(n*9,sizeof(float));
    float weights[1024];
    if(!proj) { metal_filter_destroy(filter); return 1; }
    for(int i=0;i<1024;i++) weights[i]=expf(-3*(1-(-1+2*(float)i/1023)));
    /* Exercise empty neighborhoods, dim centers, clipped image edges and
       wrap boundaries with deterministic phase/amplitude fields. */
    for(int pass=0;pass<3;pass++) {
        for(size_t f=0;f<3;f++) for(size_t i=0;i<n;i++) {
            float angle=(float)((i*17+f*23)%101)*6.28318530718f/101;
            proj[(f*3)*n+i]=cosf(angle);
            proj[(f*3+1)*n+i]=sinf(angle);
            proj[(f*3+2)*n+i]=pass==0?0:(pass==1?1:(i%7==0?0:(float)(i%31+1)));
        }
        const float *result=NULL;
        if(!metal_filter_run(filter,proj,weights,511.5f,2,&result)) { free(proj);metal_filter_destroy(filter);return 1; }
        for(size_t f=0;f<3;f++) for(unsigned y=0;y<height;y++) for(unsigned x=0;x<width;x++) {
            size_t i=(size_t)y*width+x,b=f*3*n;
            float m=proj[b+2*n+i],sx=0,sy=0,sm=0,sw=0;
            if(m>0) for(int yy=(int)y-2;yy<=(int)y+2;yy++) for(int xx=(int)x-2;xx<=(int)x+2;xx++) {
                if(xx<0||yy<0||xx>=(int)width||yy>=(int)height)continue;
                size_t j=(size_t)yy*width+(unsigned)xx;
                if(proj[b+2*n+j]<=0)continue;
                float weight=1;
                if(m>2) {
                    float cosine=proj[b+i]*proj[b+j]+proj[b+n+i]*proj[b+n+j];
                    int k=(int)((cosine+1)*511.5f); if(k<0)k=0;if(k>1023)k=1023;
                    weight=weights[k];
                }
                sx+=weight*proj[b+j];sy+=weight*proj[b+n+j];sm+=weight*proj[b+2*n+j];sw+=weight;
            }
            float phase=sw>0?atan2f(sy,sx)/6.28318530718f:0;
            if(phase<0)phase+=1;
            float amplitude=sw>0?sm/sw:m;
            float error=fabsf(phase-result[f*2*n+i]);if(error>0.5f)error=1-error;
            if(!isfinite(result[f*2*n+i])||!isfinite(result[(f*2+1)*n+i])||error>0.00001f||fabsf(amplitude-result[(f*2+1)*n+i])>0.001f) {
                fprintf(stderr,"Filter mismatch %ux%u pass=%d pixel=%zu phase_error=%g\n",width,height,pass,i,error);
                free(proj);metal_filter_destroy(filter);return 1;
            }
        }
    }
    free(proj);metal_filter_destroy(filter);return 0;
}
int main(void) {
    const unsigned dims[][2]={{1,1},{2,3},{7,5},{64,48}};
    for(unsigned i=0;i<4;i++) { int result=check(dims[i][0],dims[i][1]);if(result)return result; }
    puts("Metal filter agrees with scalar reference across borders, invalid and dim samples");
    return 0;
}
