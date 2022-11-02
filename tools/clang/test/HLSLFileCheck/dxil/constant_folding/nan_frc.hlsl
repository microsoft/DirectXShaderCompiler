// RUN: %dxc -T ps_6_6 -Od %s | FileCheck %s

// CHECK: void @main
// CHECK-NOT: dx.op.unary.f32(i32 22
// frac(NaN) -> NaN
// CHECK-DAG: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0x7FF8000000000000)
// frac(inf) -> NaN
// CHECK-DAG: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 0x7FF8000000000000)
// frac(-inf) -> NaN
// CHECK-DAG: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 0x7FF8000000000000)

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
float4 main() : SV_Target {
  return float4(frac(GetNan()), frac(GetInf()), frac(GetNegInf()), 0);
}
