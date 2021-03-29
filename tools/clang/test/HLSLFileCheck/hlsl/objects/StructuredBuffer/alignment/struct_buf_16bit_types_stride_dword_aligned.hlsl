// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types  %s | FileCheck %s

// Test the scenario where structure stride is dword aligned

struct S0
{
  float16_t v1;
  float16_t2 v2;
  float16_t3 v3;
  float16_t4 v4;
};

StructuredBuffer<S0> srv0;
RWStructuredBuffer<S0> uav0;

void main(uint i : IN0)
{
   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 1, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 2, i8 3, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 2, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 2)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 6, i8 7, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 6, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 2)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 12, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 12, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav0[i] = srv0[i];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 1, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 2, i8 3, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 2, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 2)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 6, i8 7, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 6, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 2)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 12, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 12, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)   
   uav0[i+2] = srv0.Load(i+2);

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 0, i8 1, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 0, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 2, i8 3, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 2, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 2)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 6, i8 7, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 6, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 2)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 0, i32 12, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 0, i32 12, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav0[0] = srv0[0];

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 3, i32 0, i8 1, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 3, i32 0, half %{{.*}}, half undef, half undef, half undef, i8 1, i32 4)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 3, i32 2, i8 3, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 3, i32 2, half %{{.*}}, half %{{.*}}, half undef, half undef, i8 3, i32 2)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 3, i32 6, i8 7, i32 2)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 3, i32 6, half %{{.*}}, half %{{.*}}, half %{{.*}}, half undef, i8 7, i32 2)

   // CHECK: call %dx.types.ResRet.f16 @dx.op.rawBufferLoad.f16(i32 139, %dx.types.Handle %{{.*}}, i32 3, i32 12, i8 15, i32 4)
   // CHECK: call void @dx.op.rawBufferStore.f16(i32 140, %dx.types.Handle %{{.*}}, i32 3, i32 12, half %{{.*}}, half %{{.*}}, half %{{.*}}, half %{{.*}}, i8 15, i32 4)
   uav0[3] = srv0[3];
}
