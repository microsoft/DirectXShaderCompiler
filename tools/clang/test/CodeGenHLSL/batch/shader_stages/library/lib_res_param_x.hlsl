// RUN: %dxc -T lib_6_x  %s | FileCheck %s

// resources in return/params allowed for lib_6_x
// CHECK: alloca %struct.T
// CHECK: store %struct.RWByteAddressBuffer
// CHECK: call void @"\01?resStruct@@YA?AUT2@@UT@@V?$vector@I$01@@@Z"(%struct.T2
// CHECK: %[[ptr:[^, ]+]] = getelementptr inbounds %struct.T2
// CHECK: %[[val:[^, ]+]] = load %"class.RWStructuredBuffer<D>", %"class.RWStructuredBuffer<D>"* %[[ptr]]
// CHECK: call %dx.types.Handle @"dx.op.createHandleForLib.class.RWStructuredBuffer<D>"(i32 160, %"class.RWStructuredBuffer<D>" %[[val]])


struct T {
RWByteAddressBuffer outputBuffer;
RWByteAddressBuffer outputBuffer2;
};

struct D {
  float4 a;
  int4 b;
};

struct T2 {
   RWStructuredBuffer<D> uav;
};

T2 resStruct(T t, uint2 id);

RWByteAddressBuffer outputBuffer;
RWByteAddressBuffer outputBuffer2;

[numthreads(8, 8, 1)]
void main( uint2 id : SV_DispatchThreadID )
{
    T t = {outputBuffer,outputBuffer2};
    T2 t2 = resStruct(t, id);
    uint counter = t2.uav.IncrementCounter();
    t2.uav[counter].b.xy = id;
}