// RUN: %dxc -T ps_6_6 -Od %s | FileCheck %s

// CHECK: void @main
// CHECK-NOT: dx.op.dot4.f32(i32 56
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
  return dot(float4(1,1,1,1), float4(GetNan(),1,1,1));
}
