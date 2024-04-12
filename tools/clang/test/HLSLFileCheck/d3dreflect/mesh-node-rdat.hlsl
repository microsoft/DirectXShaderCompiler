// RUN: %dxc -T lib_6_9  %s | %D3DReflect %s | FileCheck %s


// CHECK:DxilRuntimeData (size = {{[0-9]+}} bytes):
// CHECK:  StringBuffer (size = {{[0-9]+}} bytes)
// CHECK:  IndexTable (size = {{[0-9]+}} bytes)
// CHECK:  RawBytes (size = {{[0-9]+}} bytes)
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) FunctionTable[1] = {
// CHECK:    <0:RuntimeDataFunctionInfo{{.*}}> = {
// CHECK:      Name: "meshNodesLeaf"
// CHECK:      UnmangledName: "meshNodesLeaf"
// CHECK:      Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:      FunctionDependencies: <string[0]> = {}
// CHECK:      ShaderKind: Node
// CHECK:      PayloadSizeInBytes: 0
// CHECK:      AttributeSizeInBytes: 0
// CHECK:      FeatureInfo1: 0
// CHECK:      FeatureInfo2: 0
// CHECK:      ShaderStageFlag: (Node)
// CHECK:      MinShaderTarget: 0xf0069
// CHECK:      MinimumExpectedWaveLaneCount: 0
// CHECK:      MaximumExpectedWaveLaneCount: 0
// CHECK:      ShaderFlags: (OutputPositionPresent)
// CHECK:      Node: <0:NodeShaderInfo> = {
// CHECK:        LaunchType: Mesh
// CHECK:        GroupSharedBytesUsed: 0
// CHECK:        Attribs: <12:RecordArrayRef<NodeShaderFuncAttrib>[4]>  = {
// CHECK:          [0]: <0:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <0:NodeID> = {
// CHECK:              Name: "meshNodesLeaf"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// CHECK:          [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:          }
// CHECK:          [2]: <2:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: MaxDispatchGrid
// CHECK:            MaxDispatchGrid: <4:array[3]> = { 8, 1, 1 }
// CHECK:          }
// CHECK:          [3]: <3:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: MeshShaderInfo
// CHECK:            MeshShaderInfo: <0:MSInfo> = {
// CHECK:              SigOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[1]>  = {
// CHECK:                [0]: <0:SignatureElement> = {
// CHECK:                  SemanticName: "SV_Position"
// CHECK:                  SemanticIndices: <8:array[1]> = { 0 }
// CHECK:                  SemanticKind: Position
// CHECK:                  ComponentType: F32
// CHECK:                  InterpolationMode: LinearNoperspective
// CHECK:                  StartRow: 0
// CHECK:                  ColsAndStream: 3
// CHECK:                  UsageAndDynIndexMasks: 15
// CHECK:                }
// CHECK:              }
// CHECK:              SigPrimOutputElements: <{{[0-9]+}}:RecordArrayRef<SignatureElement>[1]>  = {
// CHECK:                [0]: <1:SignatureElement> = {
// CHECK:                  SemanticName: "CLR"
// CHECK:                  SemanticIndices: <8:array[1]> = { 0 }
// CHECK:                  SemanticKind: Arbitrary
// CHECK:                  ComponentType: F32
// CHECK:                  InterpolationMode: Constant
// CHECK:                  StartRow: 0
// CHECK:                  ColsAndStream: 3
// CHECK:                  UsageAndDynIndexMasks: 15
// CHECK:                }
// CHECK:              }
// CHECK:              ViewIDOutputMask: <0:bytes[0]>
// CHECK:              ViewIDPrimOutputMask: <0:bytes[0]>
// CHECK:              NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:              GroupSharedBytesUsed: 0
// CHECK:              GroupSharedBytesDependentOnViewID: 0
// CHECK:              PayloadSizeInBytes: 0
// CHECK:              MaxOutputVertices: 3
// CHECK:              MaxOutputPrimitives: 1
// CHECK:              MeshOutputTopology: 2
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:        Inputs: <{{[0-9]+}}:RecordArrayRef<IONode>[1]>  = {
// CHECK:          [0]: <0:IONode> = {
// CHECK:            IOFlagsAndKind: 97
// CHECK:            Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderIOAttrib>[3]>  = {
// CHECK:              [0]: <0:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordSizeInBytes
// CHECK:                RecordSizeInBytes: 12
// CHECK:              }
// CHECK:              [1]: <1:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordDispatchGrid
// CHECK:                RecordDispatchGrid: <RecordDispatchGrid>
// CHECK:                  ByteOffset: 0
// CHECK:                  ComponentNumAndType: 23
// CHECK:              }
// CHECK:              [2]: <2:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordAlignmentInBytes
// CHECK:                RecordAlignmentInBytes: 4
// CHECK:              }
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:      }
// CHECK:    }
// CHECK:  }
// skip over the rest of the RDAT tables
// CHECK:ID3D12LibraryReflection:
// CHECK:  D3D12_LIBRARY_DESC:
// CHECK:    Creator: <nullptr>
// CHECK:    Flags: 0
// CHECK:    FunctionCount: 1
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: meshNodesLeaf
// CHECK:      Shader Version: <unknown> 6.9
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE


struct meshNodesRecord
{
    uint3 grid : SV_DispatchGrid;
};

struct MS_OUT_POS
{
    float4 pos : SV_POSITION;
};

struct MS_OUT_CLR
{
    float4 clr : CLR;
};

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("triangle")]
[numthreads(1,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void meshNodesLeaf(
    DispatchNodeInputRecord<meshNodesRecord> inputData,
    out vertices MS_OUT_POS verts[3],
    out primitives MS_OUT_CLR prims[1],
    out indices uint3 idx[1]
)   
{
    SetMeshOutputCounts(3,1);
    verts[0].pos = float4(-1, 1, 0, 1);
    verts[1].pos = float4(1, 1, 0, 1);
    verts[2].pos = float4(1, -1, 0, 1);
    prims[0].clr = float4(1,1,1,1);
    idx[0] = uint3(0,1,2);
}                  

