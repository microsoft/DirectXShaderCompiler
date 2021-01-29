// RUN: %dxc -O0 -T cs_6_4 %s | FileCheck %s
// CHECK: fadd
// CHECK: fadd
// CHECK: fadd

RWBuffer<float> buf;

[numthreads(8, 8, 1)]
void main(uint id : SV_DispatchThreadId) {
    float x = 0;
#if defined(__SHADER_TARGET_STAGE) && __SHADER_TARGET_STAGE == __SHADER_STAGE_COMPUTE
    x += 1;
#else
    x -= 1;
#endif
#if defined(__SHADER_TARGET_MAJOR) && __SHADER_TARGET_MAJOR == 6
    x += 1;
#else
    x -= 1;
#endif
#if defined(__SHADER_TARGET_MINOR) && __SHADER_TARGET_MINOR == 4
    x += 1;
#else
    x -= 1;
#endif
    buf[id] = x;
}