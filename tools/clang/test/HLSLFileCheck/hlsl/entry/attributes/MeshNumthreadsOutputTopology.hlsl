// RUN: %dxc -T lib_6_3 %s | FileCheck %s

// CHECK: error: mesh entry point must have the numthreads attribute
// CHECK: error: mesh entry point must have the outputtopology attribute


struct myvert {
    float4 position : SV_Position;
};


[shader("mesh")]
//[NumThreads(8, 8, 2)]
//[OutputTopology("triangle")]
void main(out vertices myvert verts[32],
          uint ix : SV_GroupIndex) {
  SetMeshOutputCounts(32, 16);
  myvert v = {0.0, 0.0, 0.0, 0.0};
  verts[ix] = v;
}
