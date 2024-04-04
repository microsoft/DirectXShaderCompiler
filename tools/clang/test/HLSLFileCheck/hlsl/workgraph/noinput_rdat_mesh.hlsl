// RUN: %dxilver 1.9 | %dxc -T lib_6_9 %s | %D3DReflect %s | FileCheck %s

// ==================================================================
// Simple check of RDAT output for nodes without inputs
// ==================================================================

// CHECK:DxilRuntimeData (size = {{[0-9]+}} bytes):
// CHECK:  StringBuffer (size = {{[0-9]+}} bytes)
// CHECK:  IndexTable (size = {{[0-9]+}} bytes)
// CHECK:  RawBytes (size = {{[0-9]+}} bytes)
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) FunctionTable[1] = {
// CHECK:    <{{[0-9]+}}:RuntimeDataFunctionInfo{{.*}}> = {
// CHECK:      Name: "noinput_mesh"
// CHECK:      UnmangledName: "noinput_mesh"
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
// CHECK:      ShaderFlags: 0 (None)
// CHECK:      Node: <0:NodeShaderInfo> = {
// CHECK:        LaunchType: Mesh
// CHECK:        GroupSharedBytesUsed: 0
// CHECK:        Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[4]>  = {
// CHECK:          [0]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <{{[0-9]+}}:NodeID> = {
// CHECK:              Name: "noinput_mesh"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// CHECK:          [1]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:          }
// CHECK:          [2]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: DispatchGrid
// CHECK:            DispatchGrid: <4:array[3]> = { 4, 1, 1 }
// CHECK:          }
// CHECK:          [3]: <{{[0-9]+}}:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: MeshShaderInfo
// CHECK:            MeshShaderInfo: <0:MSInfo> = {
// CHECK:              SigOutputElements: <RecordArrayRef<SignatureElement>[0]> = {}
// CHECK:              SigPrimOutputElements: <RecordArrayRef<SignatureElement>[0]> = {}
// CHECK:              ViewIDOutputMask: <0:bytes[0]>
// CHECK:              ViewIDPrimOutputMask: <0:bytes[0]>
// CHECK:              NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:              GroupSharedBytesUsed: 0
// CHECK:              GroupSharedBytesDependentOnViewID: 0
// CHECK:              PayloadSizeInBytes: 0
// CHECK:              MaxOutputVertices: 0
// CHECK:              MaxOutputPrimitives: 0
// CHECK:              MeshOutputTopology: 2
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:        Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      }
// CHECK:    }
// CHECK:  }
// CHECK:ID3D12LibraryReflection:
// CHECK:  D3D12_LIBRARY_DESC:
// CHECK:    Creator: <nullptr>
// CHECK:    Flags: 0
// CHECK:    FunctionCount: 1
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: noinput_mesh
// CHECK:      Shader Version: <unknown> 6.9
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE

[Shader("node")]
[OutputTopology("triangle")]
[NodeLaunch("mesh")]
[NumThreads(1,1,1)]
[NodeDispatchGrid(4,1,1)]
void noinput_mesh() { }
