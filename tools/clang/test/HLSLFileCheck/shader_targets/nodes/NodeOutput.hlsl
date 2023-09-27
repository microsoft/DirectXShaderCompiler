// RUN: %dxc -Tlib_6_8 %s -ast-dump | FileCheck %s

// Make sure NodeOutput attribute works at AST level.

struct MY_INPUT_RECORD
    {
        float value;
        uint data;
    };

    struct MY_RECORD
    {
        uint3 dispatchGrid : SV_DispatchGrid;
        // shader arguments:
        uint foo;
        float bar;
    };
    struct MY_MATERIAL_RECORD
    {
        uint textureIndex;
        float3 normal;
    };

// CHECK:FunctionDecl 0x{{.*}} myFancyNode 'void (DispatchNodeInputRecord<MY_INPUT_RECORD>, NodeOutput<MY_RECORD>, NodeOutput<MY_RECORD>, NodeOutput<MY_MATERIAL_RECORD> [63], EmptyNodeOutput)'
// CHECK-NEXT:ParmVarDecl 0x{{.*}} myInput 'DispatchNodeInputRecord<MY_INPUT_RECORD>':'DispatchNodeInputRecord<MY_INPUT_RECORD>'
// CHECK-NEXT: HLSLMaxRecordsAttr 0x{{.*}} 4
// CHECK-NEXT: ParmVarDecl 0x{{.*}} myFascinatingNode 'NodeOutput<MY_RECORD>':'NodeOutput<MY_RECORD>'
// CHECK-NEXT: HLSLMaxRecordsAttr 0x{{.*}} 4
// CHECK-NEXT: ParmVarDecl 0x{{.*}} myRecords 'NodeOutput<MY_RECORD>':'NodeOutput<MY_RECORD>'
// CHECK-NEXT: HLSLMaxRecordsAttr 0x{{.*}} 4
// CHECK-NEXT: HLSLNodeIdAttr 0x{{.*}} "myNiftyNode" 3
// CHECK-NEXT: ParmVarDecl 0x{{.*}} col:60 myMaterials 'NodeOutput<MY_MATERIAL_RECORD> [63]'
// CHECK-NEXT:HLSLNodeArraySizeAttr 0x{{.*}} 63
// CHECK-NEXT:HLSLAllowSparseNodesAttr 0x{{.*}}
// CHECK-NEXT:HLSLMaxRecordsSharedWithAttr 0x{{.*}} myRecords
// CHECK-NEXT:ParmVarDecl 0x{{.*}} myProgressCounter 'EmptyNodeOutput'
// CHECK-NEXT: HLSLMaxRecordsAttr 0x{{.*}} 20
// CHECK-NEXT: CompoundStmt 0x
// CHECK-NEXT: HLSLNumThreadsAttr 0x{{.*}} 4 5 6
// CHECK-NEXT: HLSLNodeLaunchAttr 0x{{.*}} "broadcasting"
// CHECK-NEXT: HLSLShaderAttr 0x{{.*}} "node"
// CHECK-NEXT: HLSLNodeDispatchGridAttr 0x{{.*}} 4 2 1
    [NodeDispatchGrid(4,2,1)]
    [Shader("node")]
    [NodeLaunch("broadcasting")]
    [NumThreads(4,5,6)]
    void myFancyNode(

        [MaxRecords(4)]  DispatchNodeInputRecord<MY_INPUT_RECORD> myInput,

        [MaxRecords(4)] NodeOutput<MY_RECORD> myFascinatingNode,

        [NodeID("myNiftyNode",3)] [MaxRecords(4)] NodeOutput<MY_RECORD> myRecords,

        // TODO: update to NodeOutputArray.
        [MaxRecordsSharedWith(myRecords)]
        [AllowSparseNodes]
        [NodeArraySize(63)] NodeOutput<MY_MATERIAL_RECORD> myMaterials[63],

        // an output that has empty record size
        [MaxRecords(20)] EmptyNodeOutput myProgressCounter
        )
    {
    }
