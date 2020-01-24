// RUN: %dxc -O0 -T as_6_6 %s | FileCheck %s
// CHECK: fadd
// CHECK: fadd
// CHECK: fadd

#define NUM_THREADS 32

struct Payload {
    float2 dummy;
    float4 pos;
    float color[2];
};

[numthreads(NUM_THREADS, 1, 1)]
void main()
{
    Payload pld;
    float x = 0;
#if defined(__SHADER_TARGET_STAGE) && __SHADER_TARGET_STAGE == __SHADER_STAGE_AMPLIFICATION
    x += 1;
#else
    x -= 1;
#endif
#if defined(__SHADER_TARGET_MAJOR) && __SHADER_TARGET_MAJOR == 6
    x += 1;
#else
    x -= 1;
#endif
#if defined(__SHADER_TARGET_MINOR) && __SHADER_TARGET_MINOR == 6
    x += 1;
#else
    x -= 1;
#endif
    pld.pos = x;
    DispatchMesh(NUM_THREADS, 1, 1, pld);
}
