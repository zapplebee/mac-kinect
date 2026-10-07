/* Include the pinned upstream implementation to access private stage functions
 * without duplicating its model ABI or altering downloaded sources. MIT upstream
 * attribution is in THIRD_PARTY.md and the fetched LICENSE. */
#include "openk4a_depth_model.c"
#include "accelerated.h"
#include "metal_filter.h"

struct accelerated_depth { metal_filter *filter; };
accelerated_depth *accelerated_depth_create(bool use_metal) {
    accelerated_depth *d=calloc(1,sizeof(*d));
    if(d && use_metal) d->filter=metal_filter_create(640,576);
    return d;
}
void accelerated_depth_destroy(accelerated_depth *d) {
    if(d) { metal_filter_destroy(d->filter); free(d); }
}
bool accelerated_depth_uses_metal(const accelerated_depth *d) { return d && d->filter; }
bool accelerated_depth_decode(accelerated_depth *d,openk4a_depth_model_t *m,const uint8_t *raw,size_t size,uint16_t *depth,openk4a_depth_times_t *times) {
    if(!d || !openk4a_depth_model_valid(m) || !raw || !depth || size!=5310760) return false;
    if(!d->filter || m->width!=640 || m->height!=576 || m->mode!=K4A_DEPTH_MODE_NFOV_UNBINNED) {
        openk4a_depth_decode_timed(m,raw,size,depth,times); return true;
    }
    uint64_t start=openk4a_now_fine_nsec();
    depth_track_temperature(m,raw,size);
    openk4a_layout_t layout=depth_layout(m);
    depth_unpack(m,raw,size,&layout);
    depth_project(m,NULL);
    uint64_t filter_start=openk4a_now_fine_nsec();
    const float *values=NULL;
    if(metal_filter_run(d->filter,m->proj,m->weight_table,m->weight_scale,m->center_low_ab_threshold,&values)) {
        for(size_t i=0;i<m->pixels;i++) {
            float phase[3],amplitude[3];
            for(size_t f=0;f<3;f++) { phase[f]=values[f*2*m->pixels+i]; amplitude[f]=values[(f*2+1)*m->pixels+i]; }
            depth_conclude(m,i,phase,amplitude,depth);
        }
    } else {
        fprintf(stderr,"Falling back to CPU filtering\n");
        metal_filter_destroy(d->filter); d->filter=NULL;
        depth_filter(m,depth);
    }
    uint64_t end=openk4a_now_fine_nsec();
    if(times) { times->projection_nsec=filter_start-start;times->filter_nsec=end-filter_start;times->total_nsec=end-start; }
    return true;
}
