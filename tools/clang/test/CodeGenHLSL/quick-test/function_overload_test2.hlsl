// RUN: %dxc -enable-16bit-types /Tps_6_2 /Emain > %s | FileCheck %s
// CHECK: define void @main()
// CHECK: entry
// CHECK: %{{[0-9]+}} = call half @dx.op.loadInput.f16(i32 4, i32 1, i32 0, i8 0, i32 undef)
// CHECK: %{{[0-9]+}} = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)

float4 foo(float v0, float v1, float v2, float v3) { return float4(v0, v0 * v1, v0 * v1 * v2, v0 * v1 * v2 * v3); }
half4 foo(half v0, half v1, half v2, half v3) { return half4(v0, v0 * v1, v0 * v1 * v2, v0 * v1 * v2 * v3); }

[RootSignature("")]
float4 main(float vf
  : A, half vh
  : B) : SV_Target{
    return foo(vf, vf, vf, vf) + foo(vh, vh, vh, vh);
}