// RUN: %dxc -T ps_6_6 -Od %s | FileCheck %s

// CHECK: void @main
// CHECK-NOT: dx.op.dot2.f32(i32 54
// CHECK: @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0x7FF8000000000000

float GetValue(float a, float b) {
  return a / b;
}
float GetNan() {
  return GetValue(0, 0);
}
float GetInf() {
  return GetValue(1, 0);
}
float GetNegInf() {
  return -GetInf();
}

[RootSignature("")]
float main() : SV_Target {
  return dot(float2(1,1), float2(GetNan(),1));
}
