// RUN: %dxc -Tlib_6_9 %s -verify 

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

        NodeOutput<MY_RECORD> myFascinatingNode, // expected-error{{parameter myFascinatingNode is a Node Output type, which is not allowed for mesh node shaders.}}

        [NodeID("myNiftyNode",3)] [MaxRecords(4)] NodeOutput<MY_RECORD> myRecords, // expected-error{{parameter myRecords is a Node Output type, which is not allowed for mesh node shaders.}}

        [MaxRecordsSharedWith(myRecords)]
        [AllowSparseNodes]
        [NodeArraySize(63)] NodeOutputArray<MY_MATERIAL_RECORD> myMaterials, // expected-error{{parameter myMaterials is a Node Output type, which is not allowed for mesh node shaders.}}

        // an output that has empty record size
        [MaxRecords(20)] EmptyNodeOutput myProgressCounter // expected-error{{parameter myProgressCounter is a Node Output type, which is not allowed for mesh node shaders.}}
        )
    {
    }
