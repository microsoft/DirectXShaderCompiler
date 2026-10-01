// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-constantColor | %FileCheck %s

// MRT at ps_6_0 without -enable-16bit-types: RTV0 is min16float4, RTV1 is
// float4. The override applies to SV_Target0 only.

// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 0, half 0xH3C00)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 1, half 0xH3C00)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 2, half 0xH3C00)
// CHECK: call void @dx.op.storeOutput.f16(i32 5, i32 0, i32 0, i8 3, half 0xH3C00)

// RTV1 stays unchanged:
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float 0.000000e+00)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float 0.000000e+00)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float 0.000000e+00)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float 0.000000e+00)

struct RTOut
{
  min16float4 h : SV_Target;
  float4 c : SV_Target1;
};

[RootSignature("")]
RTOut main() {
  RTOut rtOut;
  rtOut.h = min16float4(0, 0, 0, 0);
  rtOut.c = float4(0.f, 0.f, 0.f, 0.f);
  return rtOut;
}
