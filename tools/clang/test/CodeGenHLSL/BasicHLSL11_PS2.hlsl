// RUN: %dxc -E main -T ps_5_0 -Zi /Od %s | FileCheck %s

// CHECK: DepthOutput=0
// CHECK: SampleFrequency=1

// CHECK: NORMAL                   0                 sample
// CHECK: TEXCOORD                 0          noperspective

// CHECK: g_txDiffuse_texture_2d
// CHECK: g_samLinear_sampler

// CHECK: llvm.dbg.declare(metadata <4 x float>* %2
// CHECK: llvm.dbg.declare(metadata float* %3

// CHECK: llvm.dbg.declare(metadata <3 x float>* %0
// CHECK: llvm.dbg.declare(metadata <2 x float>* %1

// CHECK: llvm.dbg.declare(metadata [4 x float]* %vDiffuse
// CHECK: llvm.dbg.declare(metadata float* %fLighting

// CHECK: llvm.dbg.declare(metadata <4 x float>* %2
// CHECK: llvm.dbg.declare(metadata float* %3

// CHECK: DILocalVariable(tag: DW_TAG_arg_variable, name: "main.Ret"
// CHECK: DIExpression(DW_OP_bit_piece, 0, 16)
// CHECK: DIExpression(DW_OP_bit_piece, 16, 4)

// CHECK: DILocalVariable(tag: DW_TAG_arg_variable, name: "Input"
// CHECK: DIExpression(DW_OP_bit_piece, 0, 12)
// CHECK: DIExpression(DW_OP_bit_piece, 12, 8)
// CHECK: DILocalVariable(tag: DW_TAG_auto_variable, name: "vDiffuse"
// CHECK: DILocalVariable(tag: DW_TAG_auto_variable, name: "fLighting"
// CHECK: DILocalVariable(tag: DW_TAG_auto_variable, name: "Output"


//--------------------------------------------------------------------------------------
// File: BasicHLSL11_PS.hlsl
//
// The pixel shader file for the BasicHLSL11 sample.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
cbuffer cbPerObject : register( b0 )
{
    float4    g_vObjectColor    : packoffset( c0 );
};

cbuffer cbPerFrame : register( b1 )
{
    float3    g_vLightDir    : packoffset( c0 );
    float    g_fAmbient    : packoffset( c0.w );
};

//--------------------------------------------------------------------------------------
// Textures and Samplers
//--------------------------------------------------------------------------------------
Texture2D    g_txDiffuse : register( t0 );
SamplerState    g_samLinear : register( s0 );

//--------------------------------------------------------------------------------------
// Input / Output structures
//--------------------------------------------------------------------------------------
struct PS_INPUT
{
  sample          float3 vNormal    : NORMAL;
  noperspective   float2 vTexcoord  : TEXCOORD0;
};

struct PS_OUTPUT
{
  float4 c : SV_TARGET;
  float  d : SV_DEPTH;
};


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

PS_OUTPUT main( PS_INPUT Input) : SV_TARGET
{
    float4 vDiffuse = g_txDiffuse.Sample( g_samLinear, Input.vTexcoord );

    float fLighting = saturate( dot( g_vLightDir, Input.vNormal ) );
    fLighting = max( fLighting, g_fAmbient );

struct PS_OUTPUT Output;
    Output.c = vDiffuse * fLighting;
    Output.d = fLighting;
    return Output;
}

