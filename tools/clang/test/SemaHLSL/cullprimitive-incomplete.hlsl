// RUN: %dxc -T lib_6_8 -verify %s

// The SV_CullPrimitive check must not crash on incomplete primitive types.

struct Vert {
  float4 pos : SV_Position;
};

struct DefinedLater;
struct NeverDefined;
struct Incomplete;
struct HasIncompleteField {
  Incomplete inner; // expected-error {{field has incomplete type 'Incomplete'}} expected-note@-2 {{forward declaration of 'Incomplete'}}
};

[shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]
void ms_later(out vertices Vert verts[3], out indices uint3 tris[1],
              out primitives DefinedLater prims[1]) { // expected-error {{variable has incomplete type 'DefinedLater __restrict[1]'}}
  SetMeshOutputCounts(0, 0);
}

[shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]
void ms_never(out vertices Vert verts[3], out indices uint3 tris[1],
              out primitives NeverDefined prims[1]) { // expected-error {{variable has incomplete type 'NeverDefined __restrict[1]'}}
  SetMeshOutputCounts(0, 0);
}

[shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]
void ms_field(out vertices Vert verts[3], out indices uint3 tris[1],
              out primitives HasIncompleteField prims[1]) {
  SetMeshOutputCounts(0, 0);
}

struct DefinedLater {
  bool cull : SV_CullPrimitive;
};
