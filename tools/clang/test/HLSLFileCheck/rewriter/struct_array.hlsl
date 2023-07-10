// RUN: %dxr -remove-unused-functions -remove-unused-globals -E vs_main %s | FileCheck %s

// Make sure InstanceDataStructType not removed.
// CHECK: struct InstanceDataStructType {
// CHECK-NEXT:  float4 data;
// CHECK-NEXT: };
// Make sure InstanceDataStructTypeNotUsed is removed.
// CHECK-NOT:InstanceDataStructTypeNotUsed

struct InstanceDataStructType
{
    float4 data;
};

cbuffer InstanceData
{
    InstanceDataStructType mData[2];
};

struct InstanceDataStructTypeNotUsed
{
    float4 data;
};

cbuffer InstanceDataNotUsed
{
    InstanceDataStructType mDataNotUsed[2];
};

struct VS_INPUT
{
    float3 position : POSITION;    
    float4 texcoord0 : TEXCOORD0;
    float4 diffuse: COLOR0;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float4 uv : TEXCOORD0;
    float4 color : VC;
};   
 
VS_OUTPUT vs_main(VS_INPUT input, uint instanceID : SV_InstanceID)
{
    VS_OUTPUT output = (VS_OUTPUT)0;
    output.position += mData[0].data;

    return output;
}

float4 ps_main(VS_OUTPUT input) : SV_Target0 
{
    return input.color;
}
