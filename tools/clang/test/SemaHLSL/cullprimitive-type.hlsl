// RUN: %dxc -T lib_6_8 -verify %s

// SV_CullPrimitive on a mesh shader primitive output must be a scalar bool.
// A semantic on a struct applies to each of its leaf fields. Pixel shader
// inputs are replaced with 0, so any type is accepted there.

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
struct PrimBool1x2 {
  bool1x2 cull : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'bool1x2'}}
};
struct PrimIndexed {
  uint cull : SV_CullPrimitive1; // expected-error {{SV_CullPrimitive1 must be applied to a scalar bool, but 'cull' has type 'uint'}}
};
struct PrimLowercase {
  uint cull : sv_cullprimitive; // expected-error {{sv_cullprimitive must be applied to a scalar bool, but 'cull' has type 'uint'}}
};
enum CullEnum { CullOff, CullOn };
struct PrimEnum {
  CullEnum cull : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'CullEnum'}}
};
// Inner is reached by two entry points and through an array, but its error
// is reported once.
struct Inner {
  uint cull : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'uint'}}
};
struct PrimNested {
  float4 col : COLOR0;
  Inner inner;
};
struct PrimArrayOfStructs {
  Inner inner[2];
};
struct Base {
  uint cull : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'cull' has type 'uint'}}
};
struct PrimDerived : Base {};
// The semantic on 'data' overrides the one inside Overridden.
struct Overridden {
  uint cull : SV_CullPrimitive;
};
struct PrimOverride {
  Overridden data : TEXCOORD0;
};
// A semantic on a struct field applies to each leaf inside it.
struct WrapBool {
  bool value;
};
struct WrapUint {
  uint value;
};
struct WrapMixed {
  bool ok;
  uint bad;
};
struct WrapDerived : WrapUint {};
struct PrimWrapBool {
  WrapBool data : SV_CullPrimitive;
};
struct PrimWrapUint {
  WrapUint data : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'value' has type 'uint'}}
};
struct PrimWrapMixed {
  WrapMixed data : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'bad' has type 'uint'}}
};
struct PrimWrapDerived {
  WrapDerived data : SV_CullPrimitive; // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'value' has type 'uint'}}
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
MESH(ms_bool1x2, PrimBool1x2)
MESH(ms_indexed, PrimIndexed)
MESH(ms_lowercase, PrimLowercase)
MESH(ms_enum, PrimEnum)
MESH(ms_nested, PrimNested)
MESH(ms_array_of_structs, PrimArrayOfStructs)
MESH(ms_derived, PrimDerived)
MESH(ms_override, PrimOverride)
MESH(ms_wrap_bool, PrimWrapBool)
MESH(ms_wrap_uint, PrimWrapUint)
MESH(ms_wrap_mixed, PrimWrapMixed)
MESH(ms_wrap_derived, PrimWrapDerived)

// SV_CullPrimitive directly on the parameter applies to each primitive.
[shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]
void ms_param_bool(out vertices Vert verts[3], out indices uint3 tris[1],
                   out primitives bool prims[1] : SV_CullPrimitive) {
  SetMeshOutputCounts(0, 0);
}
[shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]
void ms_param_bool1(out vertices Vert verts[3], out indices uint3 tris[1],
                    out primitives bool1 prims[1] : SV_CullPrimitive) {
  SetMeshOutputCounts(0, 0);
}
[shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]
void ms_param_wrap_bool(out vertices Vert verts[3], out indices uint3 tris[1],
                        out primitives WrapBool prims[1] : SV_CullPrimitive) {
  SetMeshOutputCounts(0, 0);
}
[shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]
void ms_param_wrap_uint(out vertices Vert verts[3], out indices uint3 tris[1],
                        out primitives WrapUint prims[1] : SV_CullPrimitive) { // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'value' has type 'uint'}}
  SetMeshOutputCounts(0, 0);
}
[shader("mesh")] [numthreads(1, 1, 1)] [outputtopology("triangle")]
void ms_param_uint(out vertices Vert verts[3], out indices uint3 tris[1],
                   out primitives uint prims[1] : SV_CullPrimitive) { // expected-error {{SV_CullPrimitive must be applied to a scalar bool, but 'prims' has type 'uint'}}
  SetMeshOutputCounts(0, 0);
}

[shader("pixel")]
float4 ps_uint(uint cull : SV_CullPrimitive) : SV_Target { return cull; }
