// RUN: %dxc -T cs_6_0 -Od %s | FileCheck %s

struct MyStruct {
  float x;
};

MyStruct makeStruct(float x) {
  precise MyStruct ret;
  ret.x = x;
  return ret;
}

StructuredBuffer<MyStruct> DataIn;
RWStructuredBuffer<MyStruct> DataOut;

[numthreads(1,1,1)]
void main(uint3 dispatchid : SV_DispatchThreadID) {
  MyStruct d = makeStruct(DataIn[dispatchid.x].x);
  DataOut[dispatchid.x] = d;
}

// CHECK:  %[[Alloca:.*]] = alloca float, !dx.precise
// CHECK: %[[Buffer:[0-9]]] = call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32
// CHECK: %[[Value:[0-9]]] = extractvalue %dx.types.ResRet.f32 %[[Buffer]], 0
// CHECK: store float %[[Value]], float* %[[Alloca]], align 4, !noalias
// CHECK: %[[Value:[0-9]+]] = load float, float* %[[Alloca]]
// CHECK: call void @dx.op.bufferStore.f32(i32 69, %dx.types.Handle %{{.*}}, i32 %{{[0-9]}}, i32 0, float %[[Value]], float undef, float undef, float undef, i8 1)
