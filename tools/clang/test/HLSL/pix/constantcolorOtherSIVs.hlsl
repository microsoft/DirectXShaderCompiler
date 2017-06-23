// RUN: %dxc -Emain -Tps_6_0 %s | %opt -S -hlsl-dxil-constantColor,constant-red=1 | %FileCheck %s

// Check the write to the UAVs were unaffected:
// CHECK: %floatRWUAV_UAV_structbuf = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 1, i32 1, i1 false)
// CHECK: %uav0_UAV_2d = call %dx.types.Handle @dx.op.createHandle(i32 57, i8 1, i32 0, i32 0, i1 false)
// CHECK: call void @dx.op.textureStore.f32(i32 67, %dx.types.Handle %uav0_UAV_2d, i32 0, i32 0, i32 undef, float 1.000000e+00, float 2.000000e+00, float 3.000000e+00, float 4.000000e+00, i8 15)

// Added override output color:
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float 0x40019999A0000000)

struct PSOutput
{
  float color : SV_Target;
  float depth : SV_Depth;
};
PSOutput main() : SV_Target {
  PSOutput Output;
  Output.color = 0.f;
  Output.depth = 0.f;
  return Output;
}
