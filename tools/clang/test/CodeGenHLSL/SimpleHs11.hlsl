// RUN: %dxc -E main -T hs_6_1  %s 2>&1 | FileCheck %s

// Same as SimpleHS10.hlsl, except that now we check that the compiler didn't
// lie when it told us which overload it selected.

// CHECK: SV_TessFactor 0
// CHECK: SV_InsideTessFactor 0

// CHECK: define void @main

// CHECK: define void {{.*}}HSPerPatchFunc
// CHECK: dx.op.storePatchConstant.f32{{.*}}float 1.0
// CHECK: dx.op.storePatchConstant.f32{{.*}}float 2.0
// CHECK: dx.op.storePatchConstant.f32{{.*}}float 3.0
// CHECK: dx.op.storePatchConstant.f32{{.*}}float 4.0

//--------------------------------------------------------------------------------------
// SimpleTessellation.hlsl
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


struct PSSceneIn
{
    float4 pos     : SV_Position;
    float2 tex     : TEXCOORD0;
    float3 norm    : NORMAL;
    uint   RTIndex : SV_RenderTargetArrayIndex;
};


//////////////////////////////////////////////////////////////////////////////////////////
// Simple forwarding Tessellation shaders

struct HSPerVertexData
{
    // This is just the original vertex verbatim. In many real life cases this would be a
    // control point instead
    PSSceneIn v;
};

struct HSPerPatchData
{
    // We at least have to specify tess factors per patch
    // As we're tesselating triangles, there will be 4 tess factors
    // In real life case this might contain face normal, for example
	float	edges[3] : SV_TessFactor;
	float	inside   : SV_InsideTessFactor;
};

// This function has the same name as the patch constant function, but
// its signature prevents it from being a candidate.
float4 HSPerPatchFunc()
{
    return 1.8;
}

// The compiler should *not* select this overload as the patch constant
// function. As far as the compiler is concerned, this is *not* used as
// a patch constant function, and ergo may not conform with the
// restrictions placed on those functions. To ensure the compiler is not
// selecting this overload, the function signature has two semantic errors
// (InputPatch<..., 32> should be InputPatch<..., 3> to match the shader
// entry point; and no inout arguments to the method).
HSPerPatchData HSPerPatchFunc(const InputPatch< PSSceneIn, 32 > points)
{
  HSPerPatchData d;

  d.edges[0] = -5;
  d.edges[1] = -6;
  d.edges[2] = -7;
  d.inside = -8;

  return d;
}

// hull per-control point shader
[domain("tri")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[patchconstantfunc("HSPerPatchFunc")]
[outputcontrolpoints(3)]
HSPerVertexData main( const uint id : SV_OutputControlPointID,
                      const InputPatch< PSSceneIn, 3 > points )
{
    HSPerVertexData v;

    // Just forward the vertex
    v.v = points[ id ];

	return v;
}

HSPerPatchData HSPerPatchFunc(const InputPatch< PSSceneIn, 3 > points)
{
  HSPerPatchData d;

  d.edges[0] = 1;
  d.edges[1] = 2;
  d.edges[2] = 3;
  d.inside = 4;

  return d;
}