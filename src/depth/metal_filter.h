#ifndef MAC_KINECT_METAL_FILTER_H
#define MAC_KINECT_METAL_FILTER_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct metal_filter metal_filter;
metal_filter *metal_filter_create(unsigned width, unsigned height);
void metal_filter_destroy(metal_filter *filter);
/* Output is six planar float arrays: phase0, amplitude0, phase1, ... */
bool metal_filter_run(metal_filter *filter, const float *projection,
                      const float *weights, float weight_scale, float dim_threshold,
                      const float **result);
#ifdef __cplusplus
}
#endif
#endif
