// RUN: %dxc -Tlib_6_9 %s -verify 

// Make sure invalid input records aren't allowed for mesh node shaders


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
void myFancyNode1(
    // expected-error@+1{{'ThreadNodeInputRecord' may not be used with mesh nodes (only DispatchNodeInputRecord)}}
    ThreadNodeInputRecord<MY_MATERIAL_RECORD> myProgressCounter1        
    )
{
}

[Shader("node")]
[OutputTopology("line")]
// expected-note@+1{{Launch type defined here}}
[NodeLaunch("mesh")]
[NumThreads(4,5,6)]
[NodeDispatchGrid(2,2,2)]
void myFancyNode2(    
    // expected-error@+1{{'RWThreadNodeInputRecord' may not be used with mesh nodes (only DispatchNodeInputRecord)}}
    RWThreadNodeInputRecord<MY_MATERIAL_RECORD> myProgressCounter3
    )
{
}



[Shader("node")]
[OutputTopology("line")]
// expected-note@+1{{Launch type defined here}}
[NodeLaunch("mesh")]
[NumThreads(4,5,6)]
[NodeDispatchGrid(2,2,2)]
void myFancyNode3(        
    // expected-error@+1{{'GroupNodeInputRecords' may not be used with mesh nodes (only DispatchNodeInputRecord)}}
    GroupNodeInputRecords<MY_MATERIAL_RECORD> myProgressCounter6
    )
{
}


[Shader("node")]
[OutputTopology("line")]
// expected-note@+1{{Launch type defined here}}
[NodeLaunch("mesh")]
[NumThreads(4,5,6)]
[NodeDispatchGrid(2,2,2)]
void myFancyNode4(    
    // expected-error@+1{{'RWGroupNodeInputRecords' may not be used with mesh nodes (only DispatchNodeInputRecord)}}
    RWGroupNodeInputRecords<MY_MATERIAL_RECORD> myProgressCounter8
    )
{
}