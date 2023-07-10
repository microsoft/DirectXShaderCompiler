// RUN: %dxc -Emain -Tcs_6_0 %s | %FileCheck %s

// Make sure dot2 is transformed into mul.
// CHECK:fmul
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
    output[id.z] = dot(float2(0, plane.x), float2(thing.xy));
    output[id.z] = dot(float2(plane.xy), float2(thing.x, 0));
    output[id.x+1] = dot(0, plane.xy);
}
