// RUN: %dxc -T lib_6_3 -auto-binding-space 11 -keep-all-resources %s | %D3DReflect %s | FileCheck %s

struct Foo
{
    float4 a;
    uint b;
};

RWStructuredBuffer<Foo> g_buffer[2] : register(u0);
Texture2D g_test : register(t0);
SamplerState g_sampler : register(s0);

cbuffer tes2 { float a; };

RWTexture2D<uint4> g_output : register(u2);

[shader("compute")]
[numthreads(1,1,1)]
void main() { g_output[0.xx] = 0.xxxx; }

[shader("compute")]
[numthreads(1,1,1)]
void main2() { g_buffer[0][0] = (Foo) 0; }

// CHECK: ID3D12LibraryReflection:
// CHECK:   D3D12_LIBRARY_DESC:
// CHECK:     FunctionCount: 2
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: main
// CHECK:       Shader Version: Compute 6.3
// CHECK:       BoundResources: 5
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: g_output
// CHECK:         Type: D3D_SIT_UAV_RWTYPED
// CHECK:         BindPoint: 2
// CHECK:         Space: 0
// CHECK:         uFlags: (D3D_SIF_TEXTURE_COMPONENT_0 | D3D_SIF_TEXTURE_COMPONENT_1)
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: tes2
// CHECK:         BindPoint: 0
// CHECK:         Space: 11
// CHECK:         uFlags: (D3D_SIF_USERPACKED | D3D_SIF_UNUSED
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: g_sampler
// CHECK:         Type: D3D_SIT_SAMPLER
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:         uFlags: (D3D_SIF_UNUSED)
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: g_test
// CHECK:         Type: D3D_SIT_TEXTURE
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:         uFlags: (D3D_SIF_TEXTURE_COMPONENT_0 | D3D_SIF_TEXTURE_COMPONENT_1 | D3D_SIF_UNUSED)
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: g_buffer
// CHECK:         Type: D3D_SIT_UAV_RWSTRUCTURED
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:         uFlags: (D3D_SIF_UNUSED)
// CHECK:   ID3D12FunctionReflection:
// CHECK:     D3D12_FUNCTION_DESC: Name: main2
// CHECK:       Shader Version: Compute 6.3
// CHECK:       BoundResources: 5
// CHECK:     Bound Resources:
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: g_buffer
// CHECK:         Type: D3D_SIT_UAV_RWSTRUCTURED
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:         uFlags: 0
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: tes2
// CHECK:         BindPoint: 0
// CHECK:         Space: 11
// CHECK:         uFlags: (D3D_SIF_USERPACKED | D3D_SIF_UNUSED
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: g_sampler
// CHECK:         Type: D3D_SIT_SAMPLER
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:         uFlags: (D3D_SIF_UNUSED)
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: g_test
// CHECK:         Type: D3D_SIT_TEXTURE
// CHECK:         BindPoint: 0
// CHECK:         Space: 0
// CHECK:         uFlags: (D3D_SIF_TEXTURE_COMPONENT_0 | D3D_SIF_TEXTURE_COMPONENT_1 | D3D_SIF_UNUSED)
// CHECK:       D3D12_SHADER_INPUT_BIND_DESC: Name: g_output
// CHECK:         Type: D3D_SIT_UAV_RWTYPED
// CHECK:         BindPoint: 2
// CHECK:         Space: 0
// CHECK:         uFlags: (D3D_SIF_TEXTURE_COMPONENT_0 | D3D_SIF_TEXTURE_COMPONENT_1 | D3D_SIF_UNUSED)
