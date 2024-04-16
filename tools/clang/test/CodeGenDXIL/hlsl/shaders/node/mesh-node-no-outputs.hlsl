// RUN: %dxc -Tlib_6_9 %s -verify 

// REQUIRES: dxil-1-9

// Make sure output nodes aren't allowed in mesh node shaders

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

[Shader("node")]
[OutputTopology("line")]
[NodeLaunch("mesh")]
[NumThreads(4,5,6)]
[NodeDispatchGrid(2,2,2)]
void myFancyNode(

    DispatchNodeInputRecord<MY_INPUT_RECORD> myInput,
    // expected-error@+1{{parameter myFascinatingNode is a Node Output type, which is not allowed for mesh node shaders.}}
    NodeOutput<MY_RECORD> myFascinatingNode, 
    // expected-error@+1{{parameter myRecords is a Node Output type, which is not allowed for mesh node shaders.}}
    [NodeID("myNiftyNode",3)] [MaxRecords(4)] NodeOutput<MY_RECORD> myRecords, 
    // expected-error@+1{{parameter myMaterials is a Node Output type, which is not allowed for mesh node shaders.}}
    [MaxRecordsSharedWith(myRecords)] [AllowSparseNodes] [NodeArraySize(63)] NodeOutputArray<MY_MATERIAL_RECORD> myMaterials,
    // an output that has empty record size
    // expected-error@+1{{parameter myProgressCounter is a Node Output type, which is not allowed for mesh node shaders.}}
    [MaxRecords(20)] EmptyNodeOutput myProgressCounter, 
    // expected-error@+1{{parameter myProgressCounter2 is a Node Output type, which is not allowed for mesh node shaders.}}
    [MaxRecords(20)] EmptyNodeOutputArray myProgressCounter2
    )
{
}
