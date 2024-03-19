// RUN: %dxc -T lib_6_8 %s | %D3DReflect %s | FileCheck %s

// ==================================================================
// Simple check of RDAT output for nodes without inputs
// ==================================================================

// CHECK:DxilRuntimeData (size = {{[0-9]+}} bytes):
// CHECK:  StringBuffer (size = {{[0-9]+}} bytes)
// CHECK:  IndexTable (size = {{[0-9]+}} bytes)
// CHECK:  RawBytes (size = {{[0-9]+}} bytes)
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) FunctionTable[4] = {
// CHECK:   <0:RuntimeDataFunctionInfo{{.*}}> = {
// CHECK:      Name: "noinput_broadcasting"
// CHECK:      UnmangledName: "noinput_broadcasting"
// CHECK:      Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:      FunctionDependencies: <string[0]> = {}
// CHECK:      ShaderKind: Node
// CHECK:      PayloadSizeInBytes: 0
// CHECK:      AttributeSizeInBytes: 0
// CHECK:      FeatureInfo1: 0
// CHECK:      FeatureInfo2: 0
// CHECK:      ShaderStageFlag: (Node)
// CHECK:      MinShaderTarget: 0xf0068
// CHECK:      MinimumExpectedWaveLaneCount: 0
// CHECK:      MaximumExpectedWaveLaneCount: 0
// CHECK:      ShaderFlags: 0 (None)
// CHECK:      Node: <0:NodeShaderInfo> = {
// CHECK:        LaunchType: Broadcasting
// CHECK:        GroupSharedBytesUsed: 0
// CHECK:        Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[3]>  = {
// CHECK:          [0]: <0:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <0:NodeID> = {
// CHECK:              Name: "noinput_broadcasting"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// CHECK:          [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:          }
// CHECK:          [2]: <2:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: DispatchGrid
// CHECK:            DispatchGrid: <4:array[3]> = { 4, 1, 1 }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:        Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      }
// CHECK:    }
// CHECK:    <1:RuntimeDataFunctionInfo{{.*}}> = {
// CHECK:      Name: "noinput_coalescing"
// CHECK:      UnmangledName: "noinput_coalescing"
// CHECK:      Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:      FunctionDependencies: <string[0]> = {}
// CHECK:      ShaderKind: Node
// CHECK:      PayloadSizeInBytes: 0
// CHECK:      AttributeSizeInBytes: 0
// CHECK:      FeatureInfo1: 0
// CHECK:      FeatureInfo2: 0
// CHECK:      ShaderStageFlag: (Node)
// CHECK:      MinShaderTarget: 0xf0068
// CHECK:      MinimumExpectedWaveLaneCount: 0
// CHECK:      MaximumExpectedWaveLaneCount: 0
// CHECK:      ShaderFlags: 0 (None)
// CHECK:      Node: <1:NodeShaderInfo> = {
// CHECK:        LaunchType: Coalescing
// CHECK:        GroupSharedBytesUsed: 0
// CHECK:        Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[2]>  = {
// CHECK:          [0]: <3:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <1:NodeID> = {
// CHECK:              Name: "noinput_coalescing"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// CHECK:          [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:        Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      }
// CHECK:    }
// CHECK:    <2:RuntimeDataFunctionInfo{{.*}}> = {
// CHECK:      Name: "noinput_thread"
// CHECK:      UnmangledName: "noinput_thread"
// CHECK:      Resources: <RecordArrayRef<RuntimeDataResourceInfo>[0]> = {}
// CHECK:      FunctionDependencies: <string[0]> = {}
// CHECK:      ShaderKind: Node
// CHECK:      PayloadSizeInBytes: 0
// CHECK:      AttributeSizeInBytes: 0
// CHECK:      FeatureInfo1: 0
// CHECK:      FeatureInfo2: 0
// CHECK:      ShaderStageFlag: (Node)
// CHECK:      MinShaderTarget: 0xf0068
// CHECK:      MinimumExpectedWaveLaneCount: 0
// CHECK:      MaximumExpectedWaveLaneCount: 0
// CHECK:      ShaderFlags: 0 (None)
// CHECK:      Node: <2:NodeShaderInfo> = {
// CHECK:        LaunchType: Thread
// CHECK:        GroupSharedBytesUsed: 0
// CHECK:        Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[2]>  = {
// CHECK:          [0]: <4:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <2:NodeID> = {
// CHECK:              Name: "noinput_thread"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// CHECK:          [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:        Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      }
// CHECK:    }
// CHECK:    <3:RuntimeDataFunctionInfo{{.*}}> = {
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
// CHECK:      MinShaderTarget: 0xf0068
// CHECK:      MinimumExpectedWaveLaneCount: 0
// CHECK:      MaximumExpectedWaveLaneCount: 0
// CHECK:      ShaderFlags: 0 (None)
// CHECK:      Node: <3:NodeShaderInfo> = {
// CHECK:        LaunchType: Mesh
// CHECK:        GroupSharedBytesUsed: 0
// CHECK:        Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[3]>  = {
// CHECK:          [0]: <5:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <3:NodeID> = {
// CHECK:              Name: "noinput_mesh"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// CHECK:          [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:          }
// CHECK:          [2]: <2:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: DispatchGrid
// CHECK:            DispatchGrid: <4:array[3]> = { 4, 1, 1 }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:        Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      }
// CHECK:    }
// CHECK:  }
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) NodeIDTable[4] = {
// CHECK:    <0:NodeID> = {
// CHECK:      Name: "noinput_broadcasting"
// CHECK:      Index: 0
// CHECK:    }
// CHECK:    <1:NodeID> = {
// CHECK:      Name: "noinput_coalescing"
// CHECK:      Index: 0
// CHECK:    }
// CHECK:    <2:NodeID> = {
// CHECK:      Name: "noinput_thread"
// CHECK:      Index: 0
// CHECK:    }
// CHECK:    <3:NodeID> = {
// CHECK:      Name: "noinput_mesh"
// CHECK:      Index: 0
// CHECK:    }
// CHECK:  }
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) NodeShaderFuncAttribTable[6] = {
// CHECK:    <0:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: ID
// CHECK:      ID: <0:NodeID> = {
// CHECK:        Name: "noinput_broadcasting"
// CHECK:        Index: 0
// CHECK:      }
// CHECK:    }
// CHECK:    <1:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: NumThreads
// CHECK:      NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:    }
// CHECK:    <2:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: DispatchGrid
// CHECK:      DispatchGrid: <4:array[3]> = { 4, 1, 1 }
// CHECK:    }
// CHECK:    <3:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: ID
// CHECK:      ID: <1:NodeID> = {
// CHECK:        Name: "noinput_coalescing"
// CHECK:        Index: 0
// CHECK:      }
// CHECK:    }
// CHECK:    <4:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: ID
// CHECK:      ID: <2:NodeID> = {
// CHECK:        Name: "noinput_thread"
// CHECK:        Index: 0
// CHECK:      }
// CHECK:    }
// CHECK:    <5:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: ID
// CHECK:      ID: <3:NodeID> = {
// CHECK:        Name: "noinput_mesh"
// CHECK:        Index: 0
// CHECK:      }
// CHECK:    }
// CHECK:  }
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) NodeShaderInfoTable[4] = {
// CHECK:    <0:NodeShaderInfo> = {
// CHECK:      LaunchType: Broadcasting
// CHECK:      GroupSharedBytesUsed: 0
// CHECK:      Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[3]>  = {
// CHECK:        [0]: <0:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: ID
// CHECK:          ID: <0:NodeID> = {
// CHECK:            Name: "noinput_broadcasting"
// CHECK:            Index: 0
// CHECK:          }
// CHECK:        }
// CHECK:        [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: NumThreads
// CHECK:          NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:        }
// CHECK:        [2]: <2:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: DispatchGrid
// CHECK:          DispatchGrid: <4:array[3]> = { 4, 1, 1 }
// CHECK:        }
// CHECK:      }
// CHECK:      Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:    }
// CHECK:    <1:NodeShaderInfo> = {
// CHECK:      LaunchType: Coalescing
// CHECK:      GroupSharedBytesUsed: 0
// CHECK:      Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[2]>  = {
// CHECK:        [0]: <3:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: ID
// CHECK:          ID: <1:NodeID> = {
// CHECK:            Name: "noinput_coalescing"
// CHECK:            Index: 0
// CHECK:          }
// CHECK:        }
// CHECK:        [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: NumThreads
// CHECK:          NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:        }
// CHECK:      }
// CHECK:      Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:    }
// CHECK:    <2:NodeShaderInfo> = {
// CHECK:      LaunchType: Thread
// CHECK:      GroupSharedBytesUsed: 0
// CHECK:      Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[2]>  = {
// CHECK:        [0]: <4:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: ID
// CHECK:          ID: <2:NodeID> = {
// CHECK:            Name: "noinput_thread"
// CHECK:            Index: 0
// CHECK:          }
// CHECK:        }
// CHECK:        [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: NumThreads
// CHECK:          NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:        }
// CHECK:      }
// CHECK:      Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:    }
// CHECK:    <3:NodeShaderInfo> = {
// CHECK:      LaunchType: Mesh
// CHECK:      GroupSharedBytesUsed: 0
// CHECK:      Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderFuncAttrib>[3]>  = {
// CHECK:        [0]: <5:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: ID
// CHECK:          ID: <3:NodeID> = {
// CHECK:            Name: "noinput_mesh"
// CHECK:            Index: 0
// CHECK:          }
// CHECK:        }
// CHECK:        [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: NumThreads
// CHECK:          NumThreads: <0:array[3]> = { 1, 1, 1 }
// CHECK:        }
// CHECK:        [2]: <2:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: DispatchGrid
// CHECK:          DispatchGrid: <4:array[3]> = { 4, 1, 1 }
// CHECK:        }
// CHECK:      }
// CHECK:      Outputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:      Inputs: <RecordArrayRef<IONode>[0]> = {}
// CHECK:    }
// CHECK:  }
// CHECK:ID3D12LibraryReflection:
// CHECK:  D3D12_LIBRARY_DESC:
// CHECK:    Creator: <nullptr>
// CHECK:    Flags: 0
// CHECK:    FunctionCount: 4
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: noinput_broadcasting
// CHECK:      Shader Version: <unknown> 6.8
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: noinput_coalescing
// CHECK:      Shader Version: <unknown> 6.8
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: noinput_mesh
// CHECK:      Shader Version: <unknown> 6.8
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: noinput_thread
// CHECK:      Shader Version: <unknown> 6.8
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(1,1,1)]
[NodeDispatchGrid(4,1,1)]
void noinput_broadcasting() { }

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1,1,1)]
void noinput_coalescing() { }

[Shader("node")]
[NodeLaunch("thread")]
[NumThreads(1,1,1)]
void noinput_thread() { }

[Shader("node")]
[NodeLaunch("mesh")]
[NumThreads(1,1,1)]
[NodeDispatchGrid(4,1,1)]
void noinput_mesh() { }


