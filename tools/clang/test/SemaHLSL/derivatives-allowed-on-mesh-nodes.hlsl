// RUN: %dxc -Tlib_6_9 %s -verify

// expected-no-diagnostics

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
void node01_mesh_dispatch() {
  float a = 3.0;
  (void)(t.CalculateLevelOfDetail(s, a) + 
    t.CalculateLevelOfDetailUnclamped(s, a));
}

// Make sure direct call to CalculateLevelOfDetail and CalculateLevelOfDetailUnclamped get error.
