// RUN: %dxc -T lib_6_8  %s | %D3DReflect %s | FileCheck %s

// CHECK:DxilRuntimeData (size = {{[0-9]+}} bytes):
// CHECK:  StringBuffer (size = {{[0-9]+}} bytes)
// CHECK:  IndexTable (size = {{[0-9]+}} bytes)
// CHECK:  RawBytes (size = {{[0-9]+}} bytes)
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) FunctionTable[1] = {
// CHECK:    <0:RuntimeDataFunctionInfo{{.*}}> = {
// CHECK:      Name: "depth18part0_wg_63_nodes_seed_255"
// CHECK:      UnmangledName: "depth18part0_wg_63_nodes_seed_255"
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
// CHECK:        Attribs: <8:RecordArrayRef<NodeShaderFuncAttrib>[3]>  = {
// CHECK:          [0]: <0:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: ID
// CHECK:            ID: <0:NodeID> = {
// CHECK:              Name: "depth18part0_wg_63_nodes_seed_255"
// CHECK:              Index: 0
// CHECK:            }
// CHECK:          }
// CHECK:          [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: NumThreads
// CHECK:            NumThreads: <0:array[3]> = { 25, 4, 1 }
// CHECK:          }
// CHECK:          [2]: <2:NodeShaderFuncAttrib> = {
// CHECK:            AttribKind: DispatchGrid
// CHECK:            DispatchGrid: <4:array[3]> = { 2, 8, 10 }
// CHECK:          }
// CHECK:        }
// CHECK:        Outputs: <{{[0-9]+}}:RecordArrayRef<IONode>[1]>  = {
// CHECK:          [0]: <1:IONode> = {
// CHECK:            IOFlagsAndKind: 22
// CHECK:            Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderIOAttrib>[7]>  = {
// CHECK:              [0]: <2:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: OutputID
// CHECK:                OutputID: <1:NodeID> = {
// CHECK:                  Name: "OutputyMcOutputFace"
// CHECK:                  Index: 0
// CHECK:                }
// CHECK:              }
// CHECK:              [1]: <3:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: OutputArraySize
// CHECK:                OutputArraySize: 2
// CHECK:              }
// CHECK:              [2]: <4:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: MaxRecords
// CHECK:                MaxRecords: 47
// CHECK:              }
// CHECK:              [3]: <5:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: AllowSparseNodes
// CHECK:                AllowSparseNodes: 1
// CHECK:              }
// CHECK:              [4]: <6:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordSizeInBytes
// CHECK:                RecordSizeInBytes: 20
// CHECK:              }
// CHECK:              [5]: <7:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordDispatchGrid
// CHECK:                RecordDispatchGrid: <RecordDispatchGrid>
// CHECK:                  ByteOffset: 8
// CHECK:                  ComponentNumAndType: 23
// CHECK:              }
// CHECK:              [6]: <1:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordAlignmentInBytes
// CHECK:                RecordAlignmentInBytes: 4
// CHECK:              }
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:        Inputs: <{{[0-9]+}}:RecordArrayRef<IONode>[1]>  = {
// CHECK:          [0]: <0:IONode> = {
// CHECK:            IOFlagsAndKind: 97
// CHECK:            Attribs: <12:RecordArrayRef<NodeShaderIOAttrib>[2]>  = {
// CHECK:              [0]: <0:NodeShaderIOAttrib> = {
// CHECK:                AttribKind: RecordSizeInBytes
// CHECK:                RecordSizeInBytes: 8
// CHECK:              }
// CHECK:              [1]: <1:NodeShaderIOAttrib>
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:      }
// CHECK:    }
// CHECK:  }
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) NodeIDTable[2] = {
// CHECK:    <0:NodeID> = {
// CHECK:      Name: "depth18part0_wg_63_nodes_seed_255"
// CHECK:      Index: 0
// CHECK:    }
// CHECK:    <1:NodeID> = {
// CHECK:      Name: "OutputyMcOutputFace"
// CHECK:      Index: 0
// CHECK:    }
// CHECK:  }
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) NodeShaderFuncAttribTable[3] = {
// CHECK:    <0:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: ID
// CHECK:      ID: <0:NodeID> = {
// CHECK:        Name: "depth18part0_wg_63_nodes_seed_255"
// CHECK:        Index: 0
// CHECK:      }
// CHECK:    }
// CHECK:    <1:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: NumThreads
// CHECK:      NumThreads: <0:array[3]> = { 25, 4, 1 }
// CHECK:    }
// CHECK:    <2:NodeShaderFuncAttrib> = {
// CHECK:      AttribKind: DispatchGrid
// CHECK:      DispatchGrid: <4:array[3]> = { 2, 8, 10 }
// CHECK:    }
// CHECK:  }
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) NodeShaderIOAttribTable[8] = {
// CHECK:    <0:NodeShaderIOAttrib> = {
// CHECK:      AttribKind: RecordSizeInBytes
// CHECK:      RecordSizeInBytes: 8
// CHECK:    }
// CHECK:    <1:NodeShaderIOAttrib> = {
// CHECK:      AttribKind: RecordAlignmentInBytes
// CHECK:      RecordAlignmentInBytes: 4
// CHECK:    }
// CHECK:    <2:NodeShaderIOAttrib> = {
// CHECK:      AttribKind: OutputID
// CHECK:      OutputID: <1:NodeID> = {
// CHECK:        Name: "OutputyMcOutputFace"
// CHECK:        Index: 0
// CHECK:      }
// CHECK:    }
// CHECK:    <3:NodeShaderIOAttrib> = {
// CHECK:      AttribKind: OutputArraySize
// CHECK:      OutputArraySize: 2
// CHECK:    }
// CHECK:    <4:NodeShaderIOAttrib> = {
// CHECK:      AttribKind: MaxRecords
// CHECK:      MaxRecords: 47
// CHECK:    }
// CHECK:    <5:NodeShaderIOAttrib> = {
// CHECK:      AttribKind: AllowSparseNodes
// CHECK:      AllowSparseNodes: 1
// CHECK:    }
// CHECK:    <6:NodeShaderIOAttrib> = {
// CHECK:      AttribKind: RecordSizeInBytes
// CHECK:      RecordSizeInBytes: 20
// CHECK:    }
// CHECK:    <7:NodeShaderIOAttrib> = {
// CHECK:      AttribKind: RecordDispatchGrid
// CHECK:      RecordDispatchGrid: <RecordDispatchGrid>
// CHECK:        ByteOffset: 8
// CHECK:        ComponentNumAndType: 23
// CHECK:    }
// CHECK:  }
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) IONodeTable[2] = {
// CHECK:    <0:IONode> = {
// CHECK:      IOFlagsAndKind: 97
// CHECK:      Attribs: <12:RecordArrayRef<NodeShaderIOAttrib>[2]>  = {
// CHECK:        [0]: <0:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: RecordSizeInBytes
// CHECK:          RecordSizeInBytes: 8
// CHECK:        }
// CHECK:        [1]: <1:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: RecordAlignmentInBytes
// CHECK:          RecordAlignmentInBytes: 4
// CHECK:        }
// CHECK:      }
// CHECK:    }
// CHECK:    <1:IONode> = {
// CHECK:      IOFlagsAndKind: 22
// CHECK:      Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderIOAttrib>[7]>  = {
// CHECK:        [0]: <2:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: OutputID
// CHECK:          OutputID: <1:NodeID> = {
// CHECK:            Name: "OutputyMcOutputFace"
// CHECK:            Index: 0
// CHECK:          }
// CHECK:        }
// CHECK:        [1]: <3:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: OutputArraySize
// CHECK:          OutputArraySize: 2
// CHECK:        }
// CHECK:        [2]: <4:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: MaxRecords
// CHECK:          MaxRecords: 47
// CHECK:        }
// CHECK:        [3]: <5:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: AllowSparseNodes
// CHECK:          AllowSparseNodes: 1
// CHECK:        }
// CHECK:        [4]: <6:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: RecordSizeInBytes
// CHECK:          RecordSizeInBytes: 20
// CHECK:        }
// CHECK:        [5]: <7:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: RecordDispatchGrid
// CHECK:          RecordDispatchGrid: <RecordDispatchGrid>
// CHECK:            ByteOffset: 8
// CHECK:            ComponentNumAndType: 23
// CHECK:        }
// CHECK:        [6]: <1:NodeShaderIOAttrib> = {
// CHECK:          AttribKind: RecordAlignmentInBytes
// CHECK:          RecordAlignmentInBytes: 4
// CHECK:        }
// CHECK:      }
// CHECK:    }
// CHECK:  }
// CHECK:  RecordTable (stride = {{[0-9]+}} bytes) NodeShaderInfoTable[1] = {
// CHECK:    <0:NodeShaderInfo> = {
// CHECK:      LaunchType: Broadcasting
// CHECK:      GroupSharedBytesUsed: 0
// CHECK:      Attribs: <8:RecordArrayRef<NodeShaderFuncAttrib>[3]>  = {
// CHECK:        [0]: <0:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: ID
// CHECK:          ID: <0:NodeID> = {
// CHECK:            Name: "depth18part0_wg_63_nodes_seed_255"
// CHECK:            Index: 0
// CHECK:          }
// CHECK:        }
// CHECK:        [1]: <1:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: NumThreads
// CHECK:          NumThreads: <0:array[3]> = { 25, 4, 1 }
// CHECK:        }
// CHECK:        [2]: <2:NodeShaderFuncAttrib> = {
// CHECK:          AttribKind: DispatchGrid
// CHECK:          DispatchGrid: <4:array[3]> = { 2, 8, 10 }
// CHECK:        }
// CHECK:      }
// CHECK:      Outputs: <{{[0-9]+}}:RecordArrayRef<IONode>[1]>  = {
// CHECK:        [0]: <1:IONode> = {
// CHECK:          IOFlagsAndKind: 22
// CHECK:          Attribs: <{{[0-9]+}}:RecordArrayRef<NodeShaderIOAttrib>[7]>  = {
// CHECK:            [0]: <2:NodeShaderIOAttrib> = {
// CHECK:              AttribKind: OutputID
// CHECK:              OutputID: <1:NodeID> = {
// CHECK:                Name: "OutputyMcOutputFace"
// CHECK:                Index: 0
// CHECK:              }
// CHECK:            }
// CHECK:            [1]: <3:NodeShaderIOAttrib> = {
// CHECK:              AttribKind: OutputArraySize
// CHECK:              OutputArraySize: 2
// CHECK:            }
// CHECK:            [2]: <4:NodeShaderIOAttrib> = {
// CHECK:              AttribKind: MaxRecords
// CHECK:              MaxRecords: 47
// CHECK:            }
// CHECK:            [3]: <5:NodeShaderIOAttrib> = {
// CHECK:              AttribKind: AllowSparseNodes
// CHECK:              AllowSparseNodes: 1
// CHECK:            }
// CHECK:            [4]: <6:NodeShaderIOAttrib> = {
// CHECK:              AttribKind: RecordSizeInBytes
// CHECK:              RecordSizeInBytes: 20
// CHECK:            }
// CHECK:            [5]: <7:NodeShaderIOAttrib> = {
// CHECK:              AttribKind: RecordDispatchGrid
// CHECK:              RecordDispatchGrid: <RecordDispatchGrid>
// CHECK:                ByteOffset: 8
// CHECK:                ComponentNumAndType: 23
// CHECK:            }
// CHECK:            [6]: <1:NodeShaderIOAttrib> = {
// CHECK:              AttribKind: RecordAlignmentInBytes
// CHECK:              RecordAlignmentInBytes: 4
// CHECK:            }
// CHECK:          }
// CHECK:        }
// CHECK:      }
// CHECK:      Inputs: <{{[0-9]+}}:RecordArrayRef<IONode>[1]>  = {
// CHECK:        [0]: <0:IONode> = {
// CHECK:          IOFlagsAndKind: 97
// CHECK:          Attribs: <12:RecordArrayRef<NodeShaderIOAttrib>[2]>  = {
// CHECK:            [0]: <0:NodeShaderIOAttrib> = {
// CHECK:              AttribKind: RecordSizeInBytes
// CHECK:              RecordSizeInBytes: 8
// CHECK:            }
// CHECK:            [1]: <1:NodeShaderIOAttrib>
// CHECK:          }
// CHECK:        }
// CHECK:      }
// CHECK:    }
// CHECK:  }
// CHECK:ID3D12LibraryReflection1:
// CHECK:  D3D12_LIBRARY_DESC:
// CHECK:    Creator: <nullptr>
// CHECK:    Flags: 0
// CHECK:    FunctionCount: 1
// CHECK:  ID3D12FunctionReflection:
// CHECK:    D3D12_FUNCTION_DESC: Name: depth18part0_wg_63_nodes_seed_255
// CHECK:      Shader Version: Node 6.8
// CHECK:      Creator: <nullptr>
// CHECK:      Flags: 0
// CHECK:      RequiredFeatureFlags: 0
// CHECK:      ConstantBuffers: 0
// CHECK:      BoundResources: 0
// CHECK:      FunctionParameterCount: 0
// CHECK:      HasReturn: FALSE
// CHECK:ID3D12FunctionReflection1:
// CHECK:  D3D12_FUNCTION_DESC1:
// CHECK:    RootSignatureSize: 0
// CHECK:    D3D12_NODE_SHADER_DESC:
// CHECK:      D3D12_COMPUTE_SHADER_DESC:
// CHECK:        NumThreads: 25, 4, 1
// CHECK:      LaunchType: D3D12_NODE_LAUNCH_TYPE_BROADCASTING_LAUNCH
// CHECK:      IsProgramEntry: FALSE
// CHECK:      LocalRootArgumentsTableIndex: -1
// CHECK:      DispatchGrid[0]: 2
// CHECK:      DispatchGrid[1]: 8
// CHECK:      DispatchGrid[2]: 10
// CHECK:      MaxDispatchGrid[0]: 0
// CHECK:      MaxDispatchGrid[1]: 0
// CHECK:      MaxDispatchGrid[2]: 0
// CHECK:      MaxRecursionDepth: 0
// CHECK:      D3D12_NODE_ID_DESC: (ShaderId)
// CHECK:        Name: depth18part0_wg_63_nodes_seed_255
// CHECK:        ID: 0
// CHECK:      D3D12_NODE_ID_DESC: (ShaderSharedInput)
// CHECK:        Name: depth18part0_wg_63_nodes_seed_255
// CHECK:        ID: 0
// CHECK:      InputNodes: 1
// CHECK:      OutputNodes: 1
// CHECK:  Input Nodes:
// CHECK:    D3D12_NODE_DESC:
// CHECK:      Flags: 0x61
// CHECK:      Type:
// CHECK:        Size: 8
// CHECK:        Alignment: 4
// CHECK:        DispatchGrid:
// CHECK:          ByteOffset: 0
// CHECK:          ComponentType: <unknown: 0>
// CHECK:          NumComponents: 0
// CHECK:      MaxRecords: 0
// CHECK:      MaxRecordsSharedWith: -1
// CHECK:      OutputArraySize: 0
// CHECK:      AllowSparseNodes: FALSE
// CHECK:  Output Nodes:
// CHECK:    D3D12_NODE_DESC:
// CHECK:      Flags: 0x16
// CHECK:      Type:
// CHECK:        Size: 20
// CHECK:        Alignment: 4
// CHECK:        DispatchGrid:
// CHECK:          ByteOffset: 8
// CHECK:          ComponentType: D3D12_DISPATCH_COMPONENT_TYPE_U32
// CHECK:          NumComponents: 3
// CHECK:      D3D12_NODE_ID_DESC: (OutputID)
// CHECK:        Name: OutputyMcOutputFace
// CHECK:        ID: 0
// CHECK:      MaxRecords: 47
// CHECK:      MaxRecordsSharedWith: -1
// CHECK:      OutputArraySize: 2
// CHECK:      AllowSparseNodes: TRUE

struct rec0
{
    int i0;
    float f0;
};

struct rec1
{
    float f1;
    int i1;
    uint3 dg : SV_DispatchGrid;
};


[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(25, 4, 1)]
[NodeDispatchGrid(2, 8, 10)]
export void depth18part0_wg_63_nodes_seed_255(
  DispatchNodeInputRecord<rec0> InputyMcInputFace,
  [MaxRecords(47)] [AllowSparseNodes] [NodeArraySize(2)] NodeOutputArray<rec1> OutputyMcOutputFace)
{
}
