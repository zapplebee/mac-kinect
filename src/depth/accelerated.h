#ifndef MAC_KINECT_ACCELERATED_H
#define MAC_KINECT_ACCELERATED_H
#include "portable.h"
typedef struct accelerated_depth accelerated_depth;
accelerated_depth *accelerated_depth_create(bool use_metal);
void accelerated_depth_destroy(accelerated_depth *decoder);
bool accelerated_depth_decode(accelerated_depth *decoder, openk4a_depth_model_t *model,
                              const uint8_t *raw,size_t size,uint16_t *depth,openk4a_depth_times_t *times);
bool accelerated_depth_uses_metal(const accelerated_depth *decoder);
#endif
