// XFAIL:*
// TODO: enable this test after fix https://github.com/microsoft/DirectXShaderCompiler/issues/5768

// RUN: not %dxc -E main -T vs_6_8 %s 2>&1 | FileCheck %s

// CHECK: SV_BaseVertexLocation must be integer
// CHECK: SV_StartInstanceLocation must be integer

float4 main(float loc : SV_BaseVertexLocation
           , float loc2 : SV_StartInstanceLocation
           ) : SV_Position
{
    float4 r = 0;
    r += loc;
    r += loc2;
    return r;
}
