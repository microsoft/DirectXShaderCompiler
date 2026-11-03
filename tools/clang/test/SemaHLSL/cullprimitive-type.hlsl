// RUN: %dxc -T lib_6_8 -verify %s

// SV_CullPrimitive on a mesh shader primitive output must be a scalar bool.
// Pixel shader inputs are replaced with 0, so any type is accepted there.

struct Vert {
  float4 pos : SV_Position;
};

struct PrimBool {
  bool cull : SV_CullPrimitive;
};
struct PrimBool1 {
  bool1 cull : SV_CullPrimitive;
};
struct PrimBool1x1 {
  bool1x1 cull : SV_CullPrimitive;
};
struct PrimUint {
  uint cull : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'uint'}}
};
struct PrimBool2 {
  bool2 cull : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'bool2'}}
};
struct PrimArray {
  bool cull[2] : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'bool [2]'}}
};
struct Inner {
  uint cull : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'uint'}}
};
struct PrimNested {
  float4 col : COLOR0;
  Inner inner;
};

#define MESH(name, Prim)                                                       \
  [shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]          \
  void name(out vertices Vert verts[3], out indices uint3 tris[1],             \
            out primitives Prim prims[1]) {                                    \
    SetMeshOutputCounts(0, 0);                                                 \
  }

MESH(ms_bool, PrimBool)
MESH(ms_bool1, PrimBool1)
MESH(ms_bool1x1, PrimBool1x1)
MESH(ms_uint, PrimUint)
MESH(ms_bool2, PrimBool2)
MESH(ms_array, PrimArray)
MESH(ms_nested, PrimNested)

[shader("pixel")]
float4 ps_uint(uint cull : SV_CullPrimitive) : SV_Target { return cull; }
