// RUN: %dxc -T ps_6_0 -HV 202x %s | FileCheck %s

using namespace hlsl;

namespace user {
float sin(int value) { return 42; }
float call(float value) { return sin(value); }
}

float main(float value : VALUE) : SV_Target {
  return user::call(value);
}

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 4.200000e+01)
