// RUN: %dxc -enable-16bit-types -Emain -Tps_6_2 %s | %opt -S -hlsl-dxil-constantColor | %FileCheck %s

// MRT: RTV0 is float4, RTV1 is half4. The override applies to SV_Target0
// only; RTV1 stays unchanged.

// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 1.000000e+00)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float 1.000000e+00)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float 1.000000e+00)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float 1.000000e+00)

// RTV1 stays 0xH0000 (half 0.0):
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 1, i32 0, i8 0, half 0xH0000)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 1, i32 0, i8 1, half 0xH0000)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 1, i32 0, i8 2, half 0xH0000)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 1, i32 0, i8 3, half 0xH0000)

// Unused integer overloads must not remain as external declarations.
// CHECK-NOT: declare void @dx.op.storeOutput.i16
// CHECK-NOT: declare void @dx.op.storeOutput.i32

struct RTOut
{
  float4 c : SV_Target;
  half4 h : SV_Target1;
};

[RootSignature("")]
RTOut main() {
  RTOut rtOut;
  rtOut.c = float4(0.f, 0.f, 0.f, 0.f);
  rtOut.h = half4(0, 0, 0, 0);
  return rtOut;
}
