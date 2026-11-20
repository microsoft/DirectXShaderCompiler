// RUN: %dxc -T ms_6_5 -E main -verify %s

// The SV_CullPrimitive check also runs for a profile entry point, which
// reaches DiagnoseEntry by a different path than a library shader.

struct Vert {
  float4 pos : SV_Position;
};
struct Prim {
  bool ok : SV_CullPrimitive;
};
struct BadPrim {
  uint cull : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'uint'}}
};
struct Prims {
  Prim good;
  BadPrim bad;
};

[numthreads(1, 1, 1)] [outputtopology("triangle")]
void main(out vertices Vert verts[3], out indices uint3 tris[1],
          out primitives Prims prims[1]) {
  SetMeshOutputCounts(0, 0);
}
