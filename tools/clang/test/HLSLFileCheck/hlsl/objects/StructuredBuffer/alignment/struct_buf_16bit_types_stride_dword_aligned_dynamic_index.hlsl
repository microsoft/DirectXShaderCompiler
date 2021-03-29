// RUN: %dxc -E main -T vs_6_2 -enable-16bit-types  %s | FileCheck %s

// Test the scenario where structure stride is dword aligned

struct S0
{
  int16_t v0[1];
  int16_t v1[2];
  int16_t v2[3];
  int16_t v3[4];
};

StructuredBuffer<S0> srv0;
RWStructuredBuffer<S0> uav0;

void main(uint i : IN0)
{
  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)
  uav0[i].v0[i] = srv0[i].v0[i];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)
  uav0[i].v1[i] = srv0[i].v1[i];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)  
  uav0[i].v2[i] = srv0[i].v2[i];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 %{{.*}}, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)  
  uav0[i].v3[i] = srv0[i].v3[i];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i8 1, i32 4)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 0, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 4)  
  uav0[i].v0[0] = srv0[i].v0[0];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 2, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 2, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)  
  uav0[i].v1[0] = srv0[i].v1[0];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 4, i8 1, i32 4)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 4, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 4)  
  uav0[i].v1[1] = srv0[i].v1[1];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 6, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 6, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)  
  uav0[i].v2[0] = srv0[i].v2[0];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i8 1, i32 4)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 8, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 4)  
  uav0[i].v2[1] = srv0[i].v2[1];
  
  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 10, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 10, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)  
  uav0[i].v2[2] = srv0[i].v2[2];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 12, i8 1, i32 4)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 12, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 4)  
  uav0[i].v3[0] = srv0[i].v3[0];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 14, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 14, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)  
  uav0[i].v3[1] = srv0[i].v3[1];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, i8 1, i32 4)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 16, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 4)    
  uav0[i].v3[2] = srv0[i].v3[2];

  // CHECK: call %dx.types.ResRet.i16 @dx.op.rawBufferLoad.i16(i32 139, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 18, i8 1, i32 2)
  // CHECK: call void @dx.op.rawBufferStore.i16(i32 140, %dx.types.Handle %{{.*}}, i32 %{{.*}}, i32 18, i16 %{{.*}}, i16 undef, i16 undef, i16 undef, i8 1, i32 2)    
  uav0[i].v3[3] = srv0[i].v3[3];
}
