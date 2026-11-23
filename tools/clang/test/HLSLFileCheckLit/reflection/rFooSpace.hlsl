// RUN: %dxc -T cs_6_0 %s -Fo %t.obj
// RUN: %dxa -dumpreflection %t.obj | FileCheck %s
// RUN: %dxc -T cs_6_2 -enable-16bit-types %s -Fo %t.native.obj
// RUN: %dxa -dumpreflection %t.native.obj | FileCheck %s

// CHECK: D3D12_SHADER_TYPE_DESC: Name: rFooSpace::SomeFooStruct
// CHECK-NEXT: Class: D3D_SVC_STRUCT
// CHECK: D3D12_SHADER_TYPE_DESC: Name: aBarSpace::SomeBarStruct
// CHECK-NEXT: Class: D3D_SVC_STRUCT

namespace rFooSpace
{
    struct SomeFooStruct
    {
        float3 myFloat3;
        float4 myFloat4;
    };
}

namespace aBarSpace
{
    struct SomeBarStruct
    {
        float3 myFloat3;
        float4 myFloat4;
    };
}

ConstantBuffer<rFooSpace::SomeFooStruct> cbFoo;
ConstantBuffer<aBarSpace::SomeBarStruct> cbBar;

RWBuffer<float4> output;

[numthreads(1,1,1)]
void main()
{
    output[0] = cbBar.myFloat4 * cbFoo.myFloat4;
}
