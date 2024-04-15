// RUN: %dxc -Tlib_6_9 %s -verify 

// Make sure RWDispatchNodeInputRecord input records are allowed for mesh node shaders

// expected-no-diagnostics

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
void myFancyNode1(
    RWDispatchNodeInputRecord<MY_MATERIAL_RECORD> myProgressCounter1        
    )
{
}

[Shader("node")]
[OutputTopology("line")]
[NodeLaunch("mesh")]
[NumThreads(4,5,6)]
[NodeDispatchGrid(2,2,2)]
void myFancyNode2(
    globallycoherent RWDispatchNodeInputRecord<MY_MATERIAL_RECORD> myProgressCounter1        
    )
{
}