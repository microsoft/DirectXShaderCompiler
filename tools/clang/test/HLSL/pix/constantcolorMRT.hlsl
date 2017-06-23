// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-constantColor | %FileCheck %s

// Check the write to the integer part was unaffected:
// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 8)

// Added override output color:
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 0, float 0x40019999A0000000)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 1, float 0x3FD99999A0000000)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 2, float 0x3FE3333340000000)
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 1, i32 0, i8 3, float 1.000000e+00)

struct RTOut
{
  int i : SV_Target;
  float4 c : SV_Target1;
};

[RootSignature("UAV(u0)")]
RTOut main()  {
  RTOut rtOut;
  rtOut.i = 8;
  rtOut.c = float4(0.f, 0.f, 0.f, 0.f);
  return rtOut;
}
