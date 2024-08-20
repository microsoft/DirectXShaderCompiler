// RUN: %dxc -E main -T hs_6_8 %s -Fo %t
// RUN: %dxa %t -dumppsv | FileCheck %s

// CHECK:DxilPipelineStateValidation:
// CHECK-NEXT: PSVRuntimeInfo:
// CHECK-NEXT:  Hull Shader
// CHECK-NEXT:  InputControlPointCount=3
// CHECK-NEXT:  OutputControlPointCount=3
// CHECK-NEXT:  Domain=tri
// CHECK-NEXT:  OutputPrimitive=triangle_cw
// CHECK-NEXT:  MinimumExpectedWaveLaneCount: 0
// CHECK-NEXT:  MaximumExpectedWaveLaneCount: 4294967295
// CHECK-NEXT:  UsesViewID: false
// CHECK-NEXT:  SigInputElements: 3
// CHECK-NEXT:  SigOutputElements: 3
// CHECK-NEXT:  SigPatchConstOrPrimElements: 2
// CHECK-NEXT:  SigInputVectors: 3
// CHECK-NEXT:  SigOutputVectors[0]: 3
// CHECK-NEXT:  SigOutputVectors[1]: 0
// CHECK-NEXT:  SigOutputVectors[2]: 0
// CHECK-NEXT:  SigOutputVectors[3]: 0
// CHECK-NEXT:  EntryFunctionName: main
// CHECK-NEXT: ResourceCount : 0
// CHECK-NEXT:  PSVSignatureElement:
// CHECK-NEXT:   SemanticName:
// CHECK-NEXT:   SemanticIndex: 0 0 1 2
// CHECK-NEXT:   IsAllocated: 1
// CHECK-NEXT:   StartRow: 0
// CHECK-NEXT:   StartCol: 0
// CHECK-NEXT:   Rows: 1
// CHECK-NEXT:   Cols: 4
// CHECK-NEXT:   SemanticKind: Position
// CHECK-NEXT:   InterpolationMode: 4
// CHECK-NEXT:   OutputStream: 0
// CHECK-NEXT:   ComponentType: 3
// CHECK-NEXT:   DynamicIndexMask: 0
// CHECK-NEXT: PSVSignatureElement:
// CHECK-NEXT:   SemanticName: TEXCOORD
// CHECK-NEXT:   SemanticIndex: 0 0
// CHECK-NEXT:   IsAllocated: 1
// CHECK-NEXT:   StartRow: 1
// CHECK-NEXT:   StartCol: 0
// CHECK-NEXT:   Rows: 1
// CHECK-NEXT:   Cols: 2
// CHECK-NEXT:   SemanticKind: Arbitrary
// CHECK-NEXT:   InterpolationMode: 2
// CHECK-NEXT:   OutputStream: 0
// CHECK-NEXT:   ComponentType: 3
// CHECK-NEXT:   DynamicIndexMask: 0
// CHECK-NEXT: PSVSignatureElement:
// CHECK-NEXT:   SemanticName: NORMAL
// CHECK-NEXT:   SemanticIndex: 0 0 1
// CHECK-NEXT:   IsAllocated: 1
// CHECK-NEXT:   StartRow: 2
// CHECK-NEXT:   StartCol: 0
// CHECK-NEXT:   Rows: 1
// CHECK-NEXT:   Cols: 3
// CHECK-NEXT:   SemanticKind: Arbitrary
// CHECK-NEXT:   InterpolationMode: 2
// CHECK-NEXT:   OutputStream: 0
// CHECK-NEXT:   ComponentType: 3
// CHECK-NEXT:   DynamicIndexMask: 0
// CHECK-NEXT: PSVSignatureElement:
// CHECK-NEXT:   SemanticName:
// CHECK-NEXT:   SemanticIndex: 0 0 1 2
// CHECK-NEXT:   IsAllocated: 1
// CHECK-NEXT:   StartRow: 0
// CHECK-NEXT:   StartCol: 0
// CHECK-NEXT:   Rows: 1
// CHECK-NEXT:   Cols: 4
// CHECK-NEXT:   SemanticKind: Position
// CHECK-NEXT:   InterpolationMode: 4
// CHECK-NEXT:   OutputStream: 0
// CHECK-NEXT:   ComponentType: 3
// CHECK-NEXT:   DynamicIndexMask: 0
// CHECK-NEXT: PSVSignatureElement:
// CHECK-NEXT:   SemanticName: TEXCOORD
// CHECK-NEXT:   SemanticIndex: 0 0
// CHECK-NEXT:   IsAllocated: 1
// CHECK-NEXT:   StartRow: 1
// CHECK-NEXT:   StartCol: 0
// CHECK-NEXT:   Rows: 1
// CHECK-NEXT:   Cols: 2
// CHECK-NEXT:   SemanticKind: Arbitrary
// CHECK-NEXT:   InterpolationMode: 2
// CHECK-NEXT:   OutputStream: 0
// CHECK-NEXT:   ComponentType: 3
// CHECK-NEXT:   DynamicIndexMask: 0
// CHECK-NEXT: PSVSignatureElement:
// CHECK-NEXT:   SemanticName: NORMAL
// CHECK-NEXT:   SemanticIndex: 0 0 1
// CHECK-NEXT:   IsAllocated: 1
// CHECK-NEXT:   StartRow: 2
// CHECK-NEXT:   StartCol: 0
// CHECK-NEXT:   Rows: 1
// CHECK-NEXT:   Cols: 3
// CHECK-NEXT:   SemanticKind: Arbitrary
// CHECK-NEXT:   InterpolationMode: 2
// CHECK-NEXT:   OutputStream: 0
// CHECK-NEXT:   ComponentType: 3
// CHECK-NEXT:   DynamicIndexMask: 0
// CHECK-NEXT: PSVSignatureElement:
// CHECK-NEXT:   SemanticName:
// CHECK-NEXT:   SemanticIndex: 0
// CHECK-NEXT:   IsAllocated: 1
// CHECK-NEXT:   StartRow: 0
// CHECK-NEXT:   StartCol: 3
// CHECK-NEXT:   Rows: 3
// CHECK-NEXT:   Cols: 1
// CHECK-NEXT:   SemanticKind: TessFactor
// CHECK-NEXT:   InterpolationMode: 0
// CHECK-NEXT:   OutputStream: 0
// CHECK-NEXT:   ComponentType: 3
// CHECK-NEXT:   DynamicIndexMask: 0
// CHECK-NEXT: PSVSignatureElement:
// CHECK-NEXT:   SemanticName:
// CHECK-NEXT:   SemanticIndex: 0
// CHECK-NEXT:   IsAllocated: 1
// CHECK-NEXT:   StartRow: 3
// CHECK-NEXT:   StartCol: 0
// CHECK-NEXT:   Rows: 1
// CHECK-NEXT:   Cols: 1
// CHECK-NEXT:   SemanticKind: InsideTessFactor
// CHECK-NEXT:   InterpolationMode: 0
// CHECK-NEXT:   OutputStream: 0
// CHECK-NEXT:   ComponentType: 3
// CHECK-NEXT:   DynamicIndexMask: 0
// CHECK-NEXT: Outputs affected by inputs as a table of bitmasks for stream 0:
// CHECK-NEXT: Inputs contributing to computation of Outputs[0]:
// CHECK-NEXT:   Outputs[0][0] dependent on Inputs : 0
// CHECK-NEXT:   Outputs[0][1] dependent on Inputs : 1
// CHECK-NEXT:   Outputs[0][2] dependent on Inputs : 2
// CHECK-NEXT:   Outputs[0][3] dependent on Inputs : 3
// CHECK-NEXT:   Outputs[0][4] dependent on Inputs : 4
// CHECK-NEXT:   Outputs[0][5] dependent on Inputs : 5
// CHECK-NEXT:   Outputs[0][6] dependent on Inputs :  None
// CHECK-NEXT:   Outputs[0][7] dependent on Inputs :  None
// CHECK-NEXT:   Outputs[0][8] dependent on Inputs : 8
// CHECK-NEXT:   Outputs[0][9] dependent on Inputs : 9
// CHECK-NEXT:   Outputs[0][10] dependent on Inputs : 10
// CHECK-NEXT:   Outputs[0][11] dependent on Inputs :  None
// CHECK-NEXT: Patch constant outputs affected by inputs as a table of bitmasks:
// CHECK-NEXT: Inputs contributing to computation of PatchConstantOutputs:
// CHECK-NEXT:   PatchConstantOutputs[0] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[1] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[2] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[3] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[4] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[5] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[6] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[7] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[8] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[9] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[10] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[11] dependent on Inputs :  None
// CHECK-NEXT:   PatchConstantOutputs[12] dependent on Inputs : 0  1  4  6  10  12  14
// CHECK-NEXT:   PatchConstantOutputs[13] dependent on Inputs : 3  4  9  10
// CHECK-NEXT:   PatchConstantOutputs[14] dependent on Inputs : 3  5  6
// CHECK-NEXT:   PatchConstantOutputs[15] dependent on Inputs : 1  2  7  8

struct PSSceneIn
{
    float4 pos  : SV_Position;
    float2 tex  : TEXCOORD0;
    float3 norm : NORMAL;
};


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
	float	edges[ 3 ]	: SV_TessFactor;
	float	inside		: SV_InsideTessFactor;
};

float4 HSPerPatchFunc()
{
    return 1.8;
}

HSPerPatchData HSPerPatchFunc( const InputPatch< PSSceneIn, 3 > points )
{
    HSPerPatchData d;

    d.edges[ 0 ] = 1;
    d.edges[ 1 ] = 1;
    d.edges[ 2 ] = 1;
    d.inside = 1;

    return d;
}


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