// RUN: %dxc -T cs_6_0 -Od %s | FileCheck %s

struct MyStruct {
  float x;
};

MyStruct makeStruct(float x) {
  precise MyStruct ret;
  ret.x = x;
  return ret;
}

precise MyStruct makeStruct2(float x) {
  MyStruct ret;
  ret.x = sin(x);
  return ret;
}

StructuredBuffer<MyStruct> DataIn;
RWStructuredBuffer<MyStruct> DataOut;

[numthreads(1,1,1)]
void main(uint3 dispatchid : SV_DispatchThreadID) {
  MyStruct d = makeStruct(DataIn[dispatchid.x].x);
  DataOut[dispatchid.x] = d;
  MyStruct d2 = makeStruct2(DataIn[dispatchid.x].x);
  DataOut[dispatchid.x*2+1] = d2;
}

// CHECK:  %[[Alloca:.*]] = alloca float, !dx.precise
// CHECK:  %[[Alloca2:.*]] = alloca float, !dx.precise
// CHECK: %[[Buffer:[0-9]]] = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32
// CHECK: %[[Value:[0-9]]] = extractvalue %dx.types.ResRet.f32 %[[Buffer]], 0
// CHECK: store float %[[Value]], float* %[[Alloca]], align 4, !noalias
// CHECK: %[[Value:[0-9]+]] = load float, float* %[[Alloca]]
// CHECK: call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %{{.*}}, i32 %{{[0-9]}}, i32 0, float %[[Value]], float undef, float undef, float undef, i8 1)

// CHECK: %[[Buffer2:[0-9]]] = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32
// CHECK: %[[Value2:[0-9]+]] = extractvalue %dx.types.ResRet.f32 %[[Buffer2]], 0
// CHECK: %[[SIN:[0-9a-zA-Z]+]] = call float @dx.op.unary.f32(i32 13, float %[[Value2]]), !dx.precise
// CHECK: store float %[[SIN]], float* %[[Alloca2]], align 4, !noalias
// CHECK: %[[Value3:[0-9]+]] = load float, float* %[[Alloca2]]
// CHECK: call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %{{.*}}, i32 %{{[0-9a-zA-Z]+}}, i32 0, float %[[Value3]], float undef, float undef, float undef, i8 1)
