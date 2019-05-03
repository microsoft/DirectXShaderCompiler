// RUN: %dxc -enable-16bit-types -Emain -Tcs_6_3 %s | %opt -S -hlsl-dxil-pix-shader-access-instrumentation,config=U0:2:10i0;.. | %FileCheck %s
// CHECK: call i32 @dx.op.atomicBinOp.i32
// CHECK: call void @dx.op.rawBufferStore.f32
// CHECK: call i32 @dx.op.atomicBinOp.i32
// CHECK: call void @dx.op.rawBufferStore.i32
// CHECK: call i32 @dx.op.atomicBinOp.i32
// CHECK: call void @dx.op.rawBufferStore.f16
// CHECK: call i32 @dx.op.atomicBinOp.i32
// CHECK: call void @dx.op.rawBufferStore.i16

struct S
{
    float4 f4;
    int4 i4;
    half hf;
    min16int hi;
};

RWStructuredBuffer<S> structuredUAV: register(u0);

[RootSignature(
    "DescriptorTable(UAV(u0, numDescriptors = 1, space = 0, offset = DESCRIPTOR_RANGE_OFFSET_APPEND))"
)]
[numthreads(1, 1, 1)]
void main()
{
    S s;
    s.f4 = float4(0, 0, 0, 0);
    s.i4 = int4(1, 1, 1, 1);
    s.hi = 2;
    s.hf = 3.;
    structuredUAV[0] = s;
}