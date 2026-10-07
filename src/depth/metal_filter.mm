#include "metal_filter.h"
#include "metal_source.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <cstring>
#include <cstdio>

struct metal_filter {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLComputePipelineState> pipeline;
    id<MTLBuffer> input, weights, output;
    unsigned width, height;
};

metal_filter *metal_filter_create(unsigned width, unsigned height) {
    @autoreleasepool {
        metal_filter *f=new metal_filter();
        f->width=width; f->height=height;
        f->device=MTLCreateSystemDefaultDevice();
        // Some command-line/login contexts enumerate a GPU without selecting a default.
        if(!f->device) f->device=MTLCopyAllDevices().firstObject;
        if(!f->device) { fprintf(stderr,"No Metal device available; using CPU\n"); delete f; return nullptr; }
        NSError *error=nil;
        MTLCompileOptions *options=[MTLCompileOptions new];
        options.fastMathEnabled=NO;
        id<MTLLibrary> library=[f->device newLibraryWithSource:[NSString stringWithUTF8String:metal_source] options:options error:&error];
        id<MTLFunction> fn=[library newFunctionWithName:@"filter_phase"];
        if(fn) f->pipeline=[f->device newComputePipelineStateWithFunction:fn error:&error];
        if(!f->pipeline) { fprintf(stderr,"Metal initialization failed: %s\n",error.localizedDescription.UTF8String); delete f; return nullptr; }
        f->queue=[f->device newCommandQueue];
        size_t n=(size_t)width*height;
        f->input=[f->device newBufferWithLength:n*9*sizeof(float) options:MTLResourceStorageModeShared];
        f->weights=[f->device newBufferWithLength:1024*sizeof(float) options:MTLResourceStorageModeShared];
        f->output=[f->device newBufferWithLength:n*6*sizeof(float) options:MTLResourceStorageModeShared];
        if(!f->queue||!f->input||!f->weights||!f->output) { delete f; return nullptr; }
        fprintf(stderr,"Metal neighborhood filter: %s\n",f->device.name.UTF8String);
        return f;
    }
}
void metal_filter_destroy(metal_filter *f) { delete f; }
bool metal_filter_run(metal_filter *f,const float *projection,const float *weights,float scale,float dim,const float **result) {
    @autoreleasepool {
        memcpy(f->input.contents,projection,f->input.length);
        memcpy(f->weights.contents,weights,f->weights.length);
        struct { uint32_t width,height; float scale,dim; } p={f->width,f->height,scale,dim};
        id<MTLCommandBuffer> command=[f->queue commandBuffer];
        id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
        if(!command||!encoder) return false;
        [encoder setComputePipelineState:f->pipeline];
        [encoder setBuffer:f->input offset:0 atIndex:0];
        [encoder setBuffer:f->weights offset:0 atIndex:1];
        [encoder setBuffer:f->output offset:0 atIndex:2];
        [encoder setBytes:&p length:sizeof(p) atIndex:3];
        NSUInteger w=f->pipeline.threadExecutionWidth;
        NSUInteger h=MIN((NSUInteger)8,f->pipeline.maxTotalThreadsPerThreadgroup/w);
        [encoder dispatchThreads:MTLSizeMake(f->width,f->height,1) threadsPerThreadgroup:MTLSizeMake(w,h,1)];
        [encoder endEncoding];
        [command commit];
        [command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted) {
            fprintf(stderr,"Metal execution failed: %s\n",command.error.localizedDescription.UTF8String);
            return false;
        }
        *result=(const float *)f->output.contents;
        return true;
    }
}
