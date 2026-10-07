// Port of OpenK4A's scalar depth_pixel neighborhood filter and atan2_turns.
// Upstream MIT attribution: see THIRD_PARTY.md and the fetched OpenK4A LICENSE.
#include <metal_stdlib>
using namespace metal;
struct Params { uint width; uint height; float scale; float dim; };

float turns(float y, float x) {
    if (x == 0.0f && y == 0.0f) return 0.0f;
    float ax=abs(x), ay=abs(y);
    float r=min(ax,ay)/max(ax,ay), r2=r*r;
    float a=-0.01172120f;
    a=a*r2+0.05265332f;
    a=a*r2-0.11643287f;
    a=a*r2+0.19354346f;
    a=a*r2-0.33262347f;
    a=a*r2+0.99997726f;
    a*=r;
    a*=0.15915494309189533577f;
    if(ay>ax) a=0.25f-a;
    if(x<0.0f) a=0.5f-a;
    if(y<0.0f) a=-a;
    return a<0.0f?a+1.0f:a;
}

kernel void filter_phase(device const float *proj [[buffer(0)]],
                         device const float *weights [[buffer(1)]],
                         device float *out [[buffer(2)]],
                         constant Params &p [[buffer(3)]],
                         uint2 xy [[thread_position_in_grid]]) {
    if(xy.x>=p.width || xy.y>=p.height) return;
    uint n=p.width*p.height, pixel=xy.y*p.width+xy.x;
    for(uint f=0;f<3;f++) {
        uint bx=f*3*n, by=bx+n, bm=by+n;
        float magnitude=proj[bm+pixel], sx=0.0f,sy=0.0f,sm=0.0f,sw=0.0f;
        if(magnitude>0.0f) {
            float cx=proj[bx+pixel],cy=proj[by+pixel];
            bool dim=magnitude<=p.dim;
            for(int row=max(0,int(xy.y)-2);row<=min(int(p.height)-1,int(xy.y)+2);row++) {
                for(int col=max(0,int(xy.x)-2);col<=min(int(p.width)-1,int(xy.x)+2);col++) {
                    uint other=uint(row)*p.width+uint(col);
                    float m=proj[bm+other];
                    if(m<=0.0f) continue;
                    float weight=1.0f;
                    if(!dim) {
                        float cosine=cx*proj[bx+other]+cy*proj[by+other];
                        int index=clamp(int((cosine+1.0f)*p.scale),0,1023);
                        weight=weights[index];
                    }
                    sx+=weight*proj[bx+other];sy+=weight*proj[by+other];
                    sm+=weight*m;sw+=weight;
                }
            }
        }
        out[(f*2)*n+pixel]=sw>0.0f?turns(sy,sx):0.0f;
        out[(f*2+1)*n+pixel]=sw>0.0f?sm/sw:magnitude;
    }
}
