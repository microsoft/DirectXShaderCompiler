// RUN: %dxilver 1.9 | %dxc -T lib_6_9 %s | %D3DReflect %s | FileCheck %s

// ==================================================================
// Simple check of RDAT output for nodes with different attrib combos
// ==================================================================

[Shader("node")]
[NodeLaunch("mesh")]
[OutputTopology("line")]
[NumThreads(1,2,3)]
[NodeDispatchGrid(4,5,6)]
[NodeIsProgramEntry]
[NodeID("some_call_me_tim", 7)]
[NodeLocalRootArgumentsTableIndex(13)]
void node01_mesh_dispatch() {}

struct RECORD {
  uint3 dg : SV_DispatchGrid;
};

[Shader("node")]
[NodeLaunch("mesh")]
[OutputTopology("triangle")]
[NodeMaxDispatchGrid(6,5,4)]
[NumThreads(3,2,1)]
[NodeShareInputOf("some_call_me_tim")]
[NodeMaxInputRecordsPerGraphEntryRecord(42, false)]
void node02_mesh_maxdispatch(DispatchNodeInputRecord<RECORD> input) {}

// CHECK:DxilRuntimeData (size = {{[0-9]+}} bytes):
// CHECK:  StringBuffer (size = {{[0-9]+}} bytes)
// CHECK:  IndexTable (size = {{[0-9]+}} bytes)
// CHECK:  RawBytes (size = {{[0-9]+}} bytes)
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) FunctionTable[2] = {
// RDAT for node01_mesh_dispatch()
// CHECK:    <{{[0-9]+}}:RuntimeDataFunctionInfo2> = {
// CHECK:      Name: "node01_mesh_dispatch"
// CHECK:      UnmangledName: "node01_mesh_dispatch"
// CHECK:      Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:      FunctionDependencies: <string[0]> = {}
// CHECK:      ShaderKind: Node
// CHECK:      PayloadSizeInBytes: 0
// CHECK:      AttributeSizeInBytes: 0
// CHECK:      FeatureInfo1: 0
// CHECK:      FeatureInfo2: 0
// set by [shader("node")]
// CHECK:      ShaderStageFlag: (Node)
// 6.9 required by mesh nodes
// CHECK:      MinShaderTarget: 0xf0069
// CHECK:      MinimumExpectedWaveLaneCount: 0
// CHECK:      MaximumExpectedWaveLaneCount: 0
// set by [NodeIsProgramEntry]
// CHECK:      ShaderFlags: (NodeProgramEntry)
// CHECK:      Node: <0:NodeShaderInfo> = {
// set by [NodeLaunch("mesh")]
// CHECK:        LaunchType: Mesh
// CHECK:        GroupSharedBytesUsed: 0
// CHECK:        Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[5]>  = {
// Renamed using [NodeID()]
// CHECK:          [0]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <{{[0-9]+}}:NodeID> = {
// CHECK:              Name: "some_call_me_tim"
// CHECK:              Index: 7
// CHECK:            }
// CHECK:          }
// set by [numthreads(1,2,3)]
// CHECK:          [1]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <0:array[3]> = { 1, 2, 3 }
// CHECK:          }
// set by [localrootargumentstableindex(13)]
// CHECK:          [2]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: LocalRootArgumentsTableIndex
// CHECK:            LocalRootArgumentsTableIndex: 13
// CHECK:          }
// set by [dispatchgrid(4,5,6)]
// CHECK:          [3]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: DispatchGrid
// CHECK:            DispatchGrid: <{{[0-9]+}}:array[3]> = { 4, 5, 6 }
// CHECK:          }
// CHECK:          [4]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: MeshShaderInfo
// CHECK:            MeshShaderInfo: <0:MSInfo> = {
// CHECK:               SigOutputElements: <RecordArrayRef<SignatureElement>[0]> = {}
// CHECK:               SigPrimOutputElements: <RecordArrayRef<SignatureElement>[0]> = {}
// CHECK:               ViewIDOutputMask: <0:bytes[0]>
// CHECK:               ViewIDPrimOutputMask: <0:bytes[0]>
// CHECK:               NumThreads: <0:array[3]> = { 1, 2, 3 }
// CHECK:               GroupSharedBytesUsed: 0
// CHECK:               GroupSharedBytesDependentOnViewID: 0
// CHECK:               PayloadSizeInBytes: 0
// CHECK:               MaxOutputVertices: 0
// CHECK:               MaxOutputPrimitives: 0
// CHECK:               MeshOutputTopology: 1
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:        Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      }
// CHECK:    }
// RDAT for node02_mesh_maxdispatch()
// CHECK:    <{{[0-9]+}}:RuntimeDataFunctionInfo2> = {
// CHECK:      Name: "node02_mesh_maxdispatch"
// CHECK:      UnmangledName: "node02_mesh_maxdispatch"
// CHECK:      Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:      FunctionDependencies: <string[0]> = {}
// CHECK:      ShaderKind: Node
// CHECK:      PayloadSizeInBytes: 0
// CHECK:      AttributeSizeInBytes: 0
// CHECK:      FeatureInfo1: 0
// CHECK:      FeatureInfo2: 0
// set by [shader("node")]
// CHECK:      ShaderStageFlag: (Node)
// 6.9 required by mesh nodes
// CHECK:      MinShaderTarget: 0xf0069
// CHECK:      MinimumExpectedWaveLaneCount: 0
// CHECK:      MaximumExpectedWaveLaneCount: 0
// CHECK:      ShaderFlags: 0 (None)
// CHECK:      Node: <{{[0-9]+}}:NodeShaderInfo> = {
// set by [NodeLaunch("mesh")]
// CHECK:        LaunchType: Mesh
// CHECK:        GroupSharedBytesUsed: 0
// CHECK:        Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[6]>  = {
// Default node ID is the function name
// CHECK:          [0]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <{{[0-9]+}}:NodeID> = {
// CHECK:              Name: "node02_mesh_maxdispatch"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// set by [numthreads(3,2,1)]
// CHECK:          [1]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <{{[0-9]+}}:array[3]> = { 3, 2, 1 }
// CHECK:          }
// set by [NodeShareInputOf("some_call_me_tim")]
// CHECK:          [2]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ShareInputOf
// CHECK:            ShareInputOf: <{{[0-9]+}}:NodeID> = {
// CHECK:              Name: "some_call_me_tim"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// set by [NodeMaxDispatchGrid(6,5,4)]
// CHECK:          [3]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: MaxDispatchGrid
// CHECK:            MaxDispatchGrid: <{{[0-9]+}}:array[3]> = { 6, 5, 4 }
// CHECK:          }
// CHECK:          [4]: <4:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: MeshShaderInfo
// CHECK:            MeshShaderInfo: <0:MSInfo> = {
// CHECK:              SigOutputElements: <RecordArrayRef<SignatureElement>[0]> = {}
// CHECK:              SigPrimOutputElements: <RecordArrayRef<SignatureElement>[0]> = {}
// CHECK:              ViewIDOutputMask: <0:bytes[0]>
// CHECK:              ViewIDPrimOutputMask: <0:bytes[0]>
// CHECK:              NumThreads: <15:array[3]> = { 3, 2, 1 }
// CHECK:              GroupSharedBytesUsed: 0
// CHECK:              GroupSharedBytesDependentOnViewID: 0
// CHECK:              PayloadSizeInBytes: 0
// CHECK:              MaxOutputVertices: 0
// CHECK:              MaxOutputPrimitives: 0
// set by [OutputTopology("triangle")] (triangle == 2)
// CHECK:              MeshOutputTopology: 2
// CHECK:            }
// CHECK:          }
// set by [NodeMaxInputRecordsPerGraphEntryRecord(42, false)]
// CHECK:          [5]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: MaxInputRecordsPerGraphEntryRecord
// CHECK:            MaxInputRecordsPerGraphEntryRecord: <{{[0-9]+}}:MaxInputRecords> = {
// CHECK:              Count: 42
// CHECK:              Shared: 0
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:        Inputs: <{{[0-9]+}}:RecordArrayRef<IONode>[1]>  = {
// CHECK:          [0]: <{{[0-9]+}}:IONode> = {
// CHECK:            IOFlagsAndKind: 97
// CHECK:            Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderIOAttrib>[3]>  = {
// CHECK:              [0]: <{{[0-9]+}}:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordSizeInBytes
// CHECK:                RecordSizeInBytes: 12
// CHECK:              }
// CHECK:              [1]: <{{[0-9]+}}:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordDispatchGrid
// CHECK:                RecordDispatchGrid: <RecordDispatchGrid>
// CHECK:                  ByteOffset: 0
// CHECK:                  ComponentNumAndType: 23
// CHECK:              }
// CHECK:              [2]: <{{[0-9]+}}:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordAlignmentInBytes
// CHECK:                RecordAlignmentInBytes: 4
// CHECK:              }
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:      }
// CHECK:    }
// CHECK:  }
// Lots of intermediate RDAT output that is skipped,
// we jump to checking for ID3D12Library Reflection here.
// CHECK:ID3D12LibraryReflection:
// CHECK:  D3D12_LIBRARY_DESC:
// CHECK:    Creator: <nullptr>
// CHECK:    Flags: 0
// CHECK:    FunctionCount: 2
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: node01_mesh_dispatch
// CHECK:      Shader Version: <unknown> 6.9
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: node02_mesh_maxdispatch
// CHECK:      Shader Version: <unknown> 6.9
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE
