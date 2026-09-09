#include "dsp/CorrectionEngine.h"
#include <cstdlib>
#include <iostream>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
namespace { bool watching=false; size_t allocations=0; }
void* operator new(size_t size) {
    if(watching) ++allocations;
    if(auto* pointer=std::malloc(size?size:1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer,size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer,size_t) noexcept { std::free(pointer); }
void* operator new(size_t size,std::align_val_t alignment) {
    if(watching) ++allocations;
    const size_t align=static_cast<size_t>(alignment);
#if defined(_WIN32)
    void* pointer=_aligned_malloc(size?size:1,align);
#else
    void* pointer=std::aligned_alloc(align,((std::max(size,size_t{1})+align-1)/align)*align);
#endif
    if(!pointer) throw std::bad_alloc();
    return pointer;
}
void* operator new[](size_t size,std::align_val_t alignment) { return ::operator new(size,alignment); }
void operator delete(void* pointer,std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}
void operator delete[](void* pointer,std::align_val_t alignment) noexcept { ::operator delete(pointer,alignment); }
void operator delete(void* pointer,size_t,std::align_val_t alignment) noexcept { ::operator delete(pointer,alignment); }
void operator delete[](void* pointer,size_t,std::align_val_t alignment) noexcept { ::operator delete(pointer,alignment); }
int main() {
    vocalpilot::CorrectionEngine engine;
    float left[512],right[512]; float* channels[]{left,right};
    for(double rate : {44100.,48000.,96000.}) {
        engine.prepare(rate);
        for(int block=0;block<1000;++block) {
            for(int i=0;i<512;++i) left[i]=right[i]=static_cast<float>(.2*std::sin(2*3.141592653589793*220*(block*512+i)/rate));
            watching=true;
            engine.process(channels,2,1+block%512,block%12,block%2!=0,static_cast<float>(block%101)/100,block%10==0);
            watching=false;
        }
    }
    std::cout<<"Core DSP operator new/new[] calls during processing: "<<allocations<<'\n';
    return allocations==0?0:1;
}
