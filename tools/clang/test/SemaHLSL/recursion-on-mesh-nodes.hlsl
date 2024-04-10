// RUN: %dxc -Tlib_6_9 %s -verify

// make sure recursion isn't allowed in mesh node shaders

SamplerComparisonState s;
Texture1D t;


[Shader("node")]
[NodeLaunch("mesh")]
[OutputTopology("line")]
[NumThreads(4,1,1)]
[NodeDispatchGrid(4,5,6)]
[NodeIsProgramEntry]
[NodeID("some_call_me_tim", 7)]
[NodeLocalRootArgumentsTableIndex(13)]
// expected-error@+2{{recursive functions are not allowed: function 'node01_mesh_dispatch' calls recursive function 'node01_mesh_dispatch'}}
// expected-note@+1{{recursive function located here:}}
void node01_mesh_dispatch() { 
 node01_mesh_dispatch();
}
