// RUN: %dxc -Emain -Tcs_6_0 %s | %FileCheck %s

// Make sure dot4 is transformed into dot3.
// CHECK:call float @dx.op.dot3.f32(i32 55
// CHECK:call float @dx.op.dot2.f32(i32 54
// CHECK:fmul
// CHECK:dx.op.bufferStore.f32(i32 69, {{.*}}, i32 0, float 0.000000e+00


cbuffer B
{
    float4 thing;
}

StructuredBuffer<float4> planes; 
RWStructuredBuffer<float> output;

[RootSignature("RootFlags(0), CBV(b0), SRV(t0), UAV(u0)")]
[numthreads(64,1,1)]
void main(uint3 id : SV_DispatchThreadID)
{    
    float4 plane = planes[id.x];
    output[id.x] = dot(plane, float4(thing.xyz, 0));
    output[id.y] = dot(plane, float4(0, thing.zw, 0));
    output[id.z] = dot(plane, float4(0, 0, thing.y, 0));
    output[id.x+1] = dot(0, plane);
}
