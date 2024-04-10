// RUN: %dxc -Tlib_6_9 %s -verify 

// Make sure that cross group sharing is disallowed in mesh node shaders


struct MY_MATERIAL_RECORD
{
    uint textureIndex;
    float3 normal;
};

[Shader("node")]
[OutputTopology("line")]
// expected-note@+1{{Launch type defined here}}
[NodeLaunch("mesh")]
[NumThreads(4,5,6)]
[NodeDispatchGrid(2,2,2)]
void myFancyNode(
    // expected-error@+1{{'RWDispatchNodeInputRecord' may not be used with mesh nodes (only DispatchNodeInputRecord)}}
    globallycoherent RWDispatchNodeInputRecord<MY_MATERIAL_RECORD> myProgressCounter 
    )
{
}
