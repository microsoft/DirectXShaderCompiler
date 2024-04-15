// RUN: %dxc -Tlib_6_9 -enable-16bit-types -select-validator internal %s | %D3DReflect %s | FileCheck %s

// This test tests that the UsageAndDynIndexMasks flag is set to non-null values for
// mesh node functions in library shaders.



// CHECK:            MeshShaderInfo: <0:MSInfo> = {
// CHECK:              SigOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[1]>  = {
// CHECK:                [0]: <0:SignatureElement> = {
// CHECK:                  SemanticName: "SV_Position"
// CHECK:                  SemanticIndices: <14:array[1]> = { 0 }
// CHECK:                  SemanticKind: Position
// CHECK:                  ComponentType: F32
// CHECK:                  InterpolationMode: LinearNoperspective
// CHECK:                  StartRow: 0
// CHECK:                  ColsAndStream: 3
// CHECK:                  UsageAndDynIndexMasks: 15
// CHECK:                }
// CHECK:              }
// CHECK:              SigPrimOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[2]>  = {
// CHECK:                [0]: <1:SignatureElement> = {
// CHECK:                  SemanticName: "CLR"
// CHECK:                  SemanticIndices: <14:array[1]> = { 0 }
// CHECK:                  SemanticKind: Arbitrary
// CHECK:                  ComponentType: F32
// CHECK:                  InterpolationMode: Constant
// CHECK:                  StartRow: 0
// CHECK:                  ColsAndStream: 3
// CHECK:                  UsageAndDynIndexMasks: 15
// CHECK:                }
// CHECK:                [1]: <2:SignatureElement> = {
// CHECK:                  SemanticName: "LOG"
// CHECK:                  SemanticIndices: <14:array[1]> = { 0 }
// CHECK:                  SemanticKind: Arbitrary
// CHECK:                  ComponentType: U32
// CHECK:                  InterpolationMode: Constant
// CHECK:                  StartRow: 1
// CHECK:                  ColsAndStream: 0
// CHECK:                  UsageAndDynIndexMasks: 1
// CHECK:                }
// CHECK:              }

// CHECK:      MeshShaderInfo: <0:MSInfo> = {
// CHECK:        SigOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[1]>  = {
// CHECK:          [0]: <0:SignatureElement> = {
// CHECK:            SemanticName: "SV_Position"
// CHECK:            SemanticIndices: <14:array[1]> = { 0 }
// CHECK:            SemanticKind: Position
// CHECK:            ComponentType: F32
// CHECK:            InterpolationMode: LinearNoperspective
// CHECK:            StartRow: 0
// CHECK:            ColsAndStream: 3
// CHECK:            UsageAndDynIndexMasks: 15
// CHECK:          }
// CHECK:        }
// CHECK:        SigPrimOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[2]>  = {
// CHECK:          [0]: <1:SignatureElement> = {
// CHECK:            SemanticName: "CLR"
// CHECK:            SemanticIndices: <14:array[1]> = { 0 }
// CHECK:            SemanticKind: Arbitrary
// CHECK:            ComponentType: F32
// CHECK:            InterpolationMode: Constant
// CHECK:            StartRow: 0
// CHECK:            ColsAndStream: 3
// CHECK:            UsageAndDynIndexMasks: 15
// CHECK:          }
// CHECK:          [1]: <2:SignatureElement> = {
// CHECK:            SemanticName: "LOG"
// CHECK:            SemanticIndices: <14:array[1]> = { 0 }
// CHECK:            SemanticKind: Arbitrary
// CHECK:            ComponentType: U32
// CHECK:            InterpolationMode: Constant
// CHECK:            StartRow: 1
// CHECK:            ColsAndStream: 0
// CHECK:            UsageAndDynIndexMasks: 1
// CHECK:          }
// CHECK:        }

// CHECK:          MeshShaderInfo: <0:MSInfo> = {
// CHECK:            SigOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[1]>  = {
// CHECK:              [0]: <0:SignatureElement> = {
// CHECK:                SemanticName: "SV_Position"
// CHECK:                SemanticIndices: <14:array[1]> = { 0 }
// CHECK:                SemanticKind: Position
// CHECK:                ComponentType: F32
// CHECK:                InterpolationMode: LinearNoperspective
// CHECK:                StartRow: 0
// CHECK:                ColsAndStream: 3
// CHECK:                UsageAndDynIndexMasks: 15
// CHECK:              }
// CHECK:            }
// CHECK:            SigPrimOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[2]>  = {
// CHECK:              [0]: <1:SignatureElement> = {
// CHECK:                SemanticName: "CLR"
// CHECK:                SemanticIndices: <14:array[1]> = { 0 }
// CHECK:                SemanticKind: Arbitrary
// CHECK:                ComponentType: F32
// CHECK:                InterpolationMode: Constant
// CHECK:                StartRow: 0
// CHECK:                ColsAndStream: 3
// CHECK:                UsageAndDynIndexMasks: 15
// CHECK:              }
// CHECK:              [1]: <2:SignatureElement> = {
// CHECK:                SemanticName: "LOG"
// CHECK:                SemanticIndices: <14:array[1]> = { 0 }
// CHECK:                SemanticKind: Arbitrary
// CHECK:                ComponentType: U32
// CHECK:                InterpolationMode: Constant
// CHECK:                StartRow: 1
// CHECK:                ColsAndStream: 0
// CHECK:                UsageAndDynIndexMasks: 1
// CHECK:              }
// CHECK:            }
// CHECK:            ViewIDOutputMask: <0:bytes[0]>
// CHECK:            ViewIDPrimOutputMask: <0:bytes[0]>
// CHECK:            NumThreads: <6:array[3]> = { 32, 1, 1 }
// CHECK:            GroupSharedBytesUsed: 0
// CHECK:            GroupSharedBytesDependentOnViewID: 0
// CHECK:            PayloadSizeInBytes: 0
// CHECK:            MaxOutputVertices: 3
// CHECK:            MaxOutputPrimitives: 1
// CHECK:            MeshOutputTopology: 2
// CHECK:          }
// CHECK:        }

// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) SignatureElementTable[3] = {
// CHECK:    <0:SignatureElement> = {
// CHECK:      SemanticName: "SV_Position"
// CHECK:      SemanticIndices: <14:array[1]> = { 0 }
// CHECK:      SemanticKind: Position
// CHECK:      ComponentType: F32
// CHECK:      InterpolationMode: LinearNoperspective
// CHECK:      StartRow: 0
// CHECK:      ColsAndStream: 3
// CHECK:      UsageAndDynIndexMasks: 15
// CHECK:    }
// CHECK:    <1:SignatureElement> = {
// CHECK:      SemanticName: "CLR"
// CHECK:      SemanticIndices: <14:array[1]> = { 0 }
// CHECK:      SemanticKind: Arbitrary
// CHECK:      ComponentType: F32
// CHECK:      InterpolationMode: Constant
// CHECK:      StartRow: 0
// CHECK:      ColsAndStream: 3
// CHECK:      UsageAndDynIndexMasks: 15
// CHECK:    }
// CHECK:    <2:SignatureElement> = {
// CHECK:      SemanticName: "LOG"
// CHECK:      SemanticIndices: <14:array[1]> = { 0 }
// CHECK:      SemanticKind: Arbitrary
// CHECK:      ComponentType: U32
// CHECK:      InterpolationMode: Constant
// CHECK:      StartRow: 1
// CHECK:      ColsAndStream: 0
// CHECK:      UsageAndDynIndexMasks: 1
// CHECK:    }
// CHECK:  }

// CHECK:    <0:MSInfo> = {
// CHECK:      SigOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[1]>  = {
// CHECK:        [0]: <0:SignatureElement> = {
// CHECK:          SemanticName: "SV_Position"
// CHECK:          SemanticIndices: <14:array[1]> = { 0 }
// CHECK:          SemanticKind: Position
// CHECK:          ComponentType: F32
// CHECK:          InterpolationMode: LinearNoperspective
// CHECK:          StartRow: 0
// CHECK:          ColsAndStream: 3
// CHECK:          UsageAndDynIndexMasks: 15
// CHECK:        }
// CHECK:      }
// CHECK:      SigPrimOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[2]>  = {
// CHECK:        [0]: <1:SignatureElement> = {
// CHECK:          SemanticName: "CLR"
// CHECK:          SemanticIndices: <14:array[1]> = { 0 }
// CHECK:          SemanticKind: Arbitrary
// CHECK:          ComponentType: F32
// CHECK:          InterpolationMode: Constant
// CHECK:          StartRow: 0
// CHECK:          ColsAndStream: 3
// CHECK:          UsageAndDynIndexMasks: 15
// CHECK:        }
// CHECK:        [1]: <2:SignatureElement> = {
// CHECK:          SemanticName: "LOG"
// CHECK:          SemanticIndices: <14:array[1]> = { 0 }
// CHECK:          SemanticKind: Arbitrary
// CHECK:          ComponentType: U32
// CHECK:          InterpolationMode: Constant
// CHECK:          StartRow: 1
// CHECK:          ColsAndStream: 0
// CHECK:          UsageAndDynIndexMasks: 1
// CHECK:        }
// CHECK:      }



//---------------------------------------------------------------------------------------------------------------------------------
// Just Mesh Bins
//---------------------------------------------------------------------------------------------------------------------------------
#ifndef JUST_MESH_BINS_MS_LOG_OFFSET
// Compile with different offsets to force generating different shader permutations
#define JUST_MESH_BINS_MS_LOG_OFFSET 0
#endif

#define JUST_MESH_BINS_MAX_MEGAPIXELS 8
#define JUST_MESH_BINS_MAX_PIXELS (JUST_MESH_BINS_MAX_MEGAPIXELS*1048576)
#define JUST_MESH_BINS_THREAD_GROUP_SIZE 32
#define JUST_MESH_BINS_MAX_GRID_SIZE_X 512
#define JUST_MESH_BINS_MAX_GRID_SIZE_Y (JUST_MESH_BINS_MAX_PIXELS/(JUST_MESH_BINS_MAX_GRID_SIZE_X*JUST_MESH_BINS_THREAD_GROUP_SIZE))

GlobalRootSignature JustMeshBinsGlobalRS = {
    "UAV(u0,space=15)," \
    "DescriptorTable(SRV(t0, space=15)),"
    "DescriptorTable(SRV(t1, space=15)),"
    "StaticSampler(s0, space=15, filter = FILTER_MIN_MAG_MIP_POINT) "
};

LocalRootSignature JustMeshBinsLocalRS =
{
    "RootConstants(num32BitConstants=2,b0,space=15)"
};

#define JustMeshBinsMSRootSig     "UAV(u0,space=15)," \
    "DescriptorTable(SRV(t0, space=15))," \
    "DescriptorTable(SRV(t1, space=15))," \
    "RootConstants(num32BitConstants=2,b0,space=15), " \
    "RootConstants(num32BitConstants=3,b1,space=15), " \
    "StaticSampler(s0, space=15, filter = FILTER_MIN_MAG_MIP_POINT)"

RWStructuredBuffer<uint> JustMeshBinsUAV : register(u0, space15);
Texture2D<float4> JustMeshBinsTexture0 : register(t0, space15);
Texture2D<float4> JustMeshBinsTexture1 : register(t1, space15);
sampler JustMeshBinsSampler : register(s0, space15);

struct JustMeshBinsNodeConstants {
    uint ALU;
    uint samples;
};

// The root signature feeds NodeConstants via local root signature, so the constants are per-node
ConstantBuffer<JustMeshBinsNodeConstants> JustMeshBinsNodeConfig : register(b0, space15);

uint PlayWithALUJustMeshBins(uint ALUIterations, uint inputVal)
{
    uint val = inputVal;
    for (uint a = 0; a < ALUIterations; a++)
    {
        val ^= inputVal + a;
    }
    return val;
}

float PlayWithTexturesJustMeshBins(uint sampleCount, float2 startUV)
{
    float2 uv = startUV;
    float fVal = 1.0f;
    for (uint s = 0; s < sampleCount; s++)
    {
        float4 data0 = JustMeshBinsTexture0.SampleLevel(JustMeshBinsSampler, uv, 0);
        float4 data1 = JustMeshBinsTexture1.SampleLevel(JustMeshBinsSampler, uv, 0);
        fVal += data0.x == data1.x ? 0 : 1;
        fVal += data0.y == data1.y ? 0 : 1;
        fVal += data0.z == data1.z ? 0 : 1;
        fVal += data0.w == data1.w ? 0 : 1;
        uv = float2(data0.x, data1.y);
    }
    return fVal;
}

struct JustMeshBinsRecord
{
    uint logOffset;
    uint16_t2 grid : SV_DispatchGrid;
    uint16_t2 lastDispatchThreadID;
};

struct JustMeshBins_MS_OUT_POS
{
    float4 pos : SV_POSITION;
};

struct JustMeshBins_MS_OUT_CLR
{
    float4 clr : CLR;
    uint logOffset : LOG;
};

struct JustMeshBins_VS_OUT_POS_CLR
{
    float4 pos : SV_POSITION;
    float4 clr : CLR;
    uint logOffset : LOG;
};


//---------------------------------------------------------------------------------------------------------------------------------
// JustMeshBinsLeaf:
// Shader that's repeated into a node array
// This shader is what gets invoked per "pixel" for the bin it gets sent to.
[Shader("node")]
[NodeLaunch("mesh")]
[NodeMaxDispatchGrid(JUST_MESH_BINS_MAX_GRID_SIZE_X,JUST_MESH_BINS_MAX_GRID_SIZE_Y,1)]
[NumThreads(JUST_MESH_BINS_THREAD_GROUP_SIZE,1,1)]
[OutputTopology("triangle")]
void justMeshBinsLeaf(
    DispatchNodeInputRecord<JustMeshBinsRecord> inputData,
    uint2 dispatchThreadID : SV_DispatchThreadID,
    uint2 groupID : SV_GroupID,
    uint groupIndex : SV_GroupIndex,
    out vertices JustMeshBins_MS_OUT_POS verts[3],
    out primitives JustMeshBins_MS_OUT_CLR prims[1],
    out indices uint3 idx[1]
)
{
    if( (inputData.Get().lastDispatchThreadID.y < dispatchThreadID.y) || 
        ((inputData.Get().lastDispatchThreadID.y == dispatchThreadID.y) && (inputData.Get().lastDispatchThreadID.x < dispatchThreadID.x)))
    {
        return;
    }

    uint pixelLocation = (groupID.y*JUST_MESH_BINS_MAX_GRID_SIZE_X + groupID.x)*JUST_MESH_BINS_THREAD_GROUP_SIZE + groupIndex;

    uint val = PlayWithALUJustMeshBins(JustMeshBinsNodeConfig.ALU, pixelLocation);
    float fVal = val;
    fVal /= 1000000;
    fVal = PlayWithTexturesJustMeshBins(JustMeshBinsNodeConfig.samples, float2(fVal,fVal));
    val = (fVal > 2.0f) ? val : 1;

    uint logOffset = inputData.Get().logOffset;
    InterlockedAdd(JustMeshBinsUAV[logOffset], val);

    SetMeshOutputCounts(3,1);
    verts[0].pos = float4(-1, 1, 0, 1);
    verts[1].pos = float4(1, 1, 0, 1);
    verts[2].pos = float4(1, -1, 0, 1);
    prims[0].clr = float4(1,1,1,1);
    prims[0].logOffset = logOffset;
    idx[0] = uint3(0,1,2);
}
