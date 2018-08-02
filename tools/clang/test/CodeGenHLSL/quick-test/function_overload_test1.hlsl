// RUN: %dxc /Tps_6_0 /Emain > %s | FileCheck %s
// CHECK: define void @main()
// CHECK: entry
// CHECK: %{{[0-9]+}} = call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)
// CHECK: %{{[0-9]+}} = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)

float4 foo(float2 v0, float2 v1) { return float4(v0 + v1, v0 - v1); }
half4 foo(half2 v0, half2 v1) { return half4(v0 + v1, v0 - v1); }
float foo(float v0, float v1) { return v0 * (v0 + v1); }
half foo(half v0, half v1) { return v0 * (v0 + v1); }

float4 main(half v0 : A, float v1 : B) : SV_Target {
  half4 h4 = half4(v0, v1, foo(v0, 1.0f), foo(v0, 1.0f));
  float4 f4 = float4(v0, v1, foo(v1, 1.0f), foo(v1, 1.0f));
  return h4 + f4;
}