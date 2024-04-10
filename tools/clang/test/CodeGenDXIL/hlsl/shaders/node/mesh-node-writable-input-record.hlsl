// RUN: %dxc -Tlib_6_9 %s -verify 

// Make sure writable input records aren't allowed for mesh node shaders


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
    // expected-error@+1{{'RWThreadNodeInputRecord' may not be used with mesh nodes (only DispatchNodeInputRecord)}}
    RWThreadNodeInputRecord<MY_MATERIAL_RECORD> myProgressCounter 
    )
{
}
