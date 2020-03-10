// RUN: %dxc -T ps_6_6 %s | %FileCheck %s

// Make sure sampler and texture get correct annotateHandle.

// CHECK-DAG:call %dx.types.Handle @dx.op.annotateHandle(i32 217, %dx.types.Handle {{.*}}, i8 0, i8 2, %dx.types.ResourceProperties { i32 9, i32 0 }
// CHECK-DAG:call %dx.types.Handle @dx.op.annotateHandle(i32 217, %dx.types.Handle {{.*}}, i8 3, i8 14, %dx.types.ResourceProperties zeroinitializer

SamplerState samplers : register(s0);
SamplerState foo() { return samplers; }
Texture2D t0 : register(t0);
[RootSignature("DescriptorTable(SRV(t0)),DescriptorTable(Sampler(s0))")]
float4 main( float2 uv : TEXCOORD ) : SV_TARGET {
  float4 val = t0.Sample(foo(), uv);
  return val;
}