// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types  %s | FileCheck %s

StructuredBuffer<float16_t> srv0;
RWStructuredBuffer<float16_t> uav0;

StructuredBuffer<float16_t2> srv1;
RWStructuredBuffer<float16_t2> uav1;

StructuredBuffer<float16_t3> srv2;
RWStructuredBuffer<float16_t3> uav2;

StructuredBuffer<float16_t4> srv3;
RWStructuredBuffer<float16_t4> uav3;

StructuredBuffer<float16_t2x3> srv4;
RWStructuredBuffer<float16_t2x3> uav4;

StructuredBuffer<float16_t3x3> srv5;
RWStructuredBuffer<float16_t3x3> uav5;

StructuredBuffer<float16_t4x3> srv6;
RWStructuredBuffer<float16_t4x3> uav6;

StructuredBuffer<float16_t4x4> srv7;
RWStructuredBuffer<float16_t4x4> uav7;

void main(uint i : IN0)
{
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 1, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)
   uav0[i] = srv0[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 1, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)
   uav0[i+2] = srv0.Load(i+2);

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 3, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 4)
   uav1[i] = srv1[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 3, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 4)
   uav1[i] = srv1.Load(i+2);

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 7, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 2)
   uav2[i] = srv2[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 7, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 2)
   uav2[i] = srv2.Load(i+2);

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav3[i] = srv3[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav3[i] = srv3.Load(i+2);

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 3, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 4)
   uav4[i] = srv4[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 3, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 4)
   uav4[i] = srv4.Load(i+2);

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 2)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 15, i32 2)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, i8 1, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)
   uav5[i] = srv5[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 2)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 15, i32 2)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, i8 1, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 2)
   uav5[i] = srv5.Load(i+2);


   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav6[i] = srv6[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav6[i] = srv6.Load(i+2);

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 24, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 24, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav7[i] = srv7[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 24, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 24, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav7[i] = srv7.Load(i+2);


   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 1, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)
   uav0[0] = srv0[0];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 3, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 4)
   uav1[0] = srv1[0];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 7, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 4)
   uav2[0] = srv2[0];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav3[0] = srv3[0];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 8, i8 3, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 8, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 4)
   uav4[0] = srv4[0];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 8, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 16, i8 1, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 16, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)
   uav5[0] = srv5[0];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 8, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 16, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 16, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav6[0] = srv6[0];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 8, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 16, i8 15, i32 4)
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 24, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 8, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 16, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 24, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav7[0] = srv7[0];
}