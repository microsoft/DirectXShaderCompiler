// RUN: %dxc -Tps_6_0 %s | FileCheck %s

// NOTE, this will generate illegal dxil operation which will be optimized later.
// Limited to release build because debug build will hit assert.
// REQUIRES: release_build

// Make sure literal argument works for fmod.
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 1.000000e+00)

float main() : SV_Target {
    return fmod(1.0, 2.0);
}
