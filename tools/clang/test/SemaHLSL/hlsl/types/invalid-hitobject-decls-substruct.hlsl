// RUN: %dxc -T lib_6_9 -DTYPE=HitStructSub -verify %s

#define PASTE_(x,y) x##y
#define PASTE(x,y) PASTE_(x,y)

#ifndef TYPE
#define TYPE HitTpl<dx::HitObject>
#endif

// Add tests for base types and instantiated template classes with HitObjects

struct HitStruct {
  float4 f;
  dx::HitObject hit;
};

struct HitStructSub : HitStruct {
  int3 is;
};

template <typename T>
struct HitTpl {
  float4 f;
  T val;
};

dx::HitObject global_hit; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}
dx::HitObject global_hit_arr[10]; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}

cbuffer BadBuffy {
  dx::HitObject cb_hit; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}
  dx::HitObject cb_hit_arr[10]; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}
};

tbuffer BadTuffy {
  dx::HitObject tb_vec; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}
  dx::HitObject tb_vec_arr[10]; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}
  TYPE tb_vec_rec; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}
  TYPE tb_vec_rec_arr[10]; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}
};

ConstantBuffer<TYPE> const_buf; // expected-error{{HitObjects in ConstantBuffers or TextureBuffers are not supported}}
TextureBuffer<TYPE> tex_buf; // expected-error{{HitObjects in ConstantBuffers or TextureBuffers are not supported}}

[shader("pixel")]
TYPE main( // expected-error{{HitObjects in entry function return type are not supported}}
  TYPE vec : V) : SV_Target { // expected-error{{HitObjects in entry function parameters are not supported}}
  return vec;
}

[shader("vertex")]
TYPE vs_main( // expected-error{{HitObjects in entry function return type are not supported}}
                     TYPE parm : P) : SV_Target { // expected-error{{HitObjects in entry function parameters are not supported}}
  parm.f = 0;
  return parm;
}


[shader("geometry")]
[maxvertexcount(3)]
void gs_point(
  // expected-error@+1{{HitObjects in entry function parameters are not supported}}
  line TYPE e,
  // expected-error@+1{{HitObjects in geometry streams are not supported}}
  inout PointStream<TYPE> OutputStream0) {}

[shader("geometry")]
[maxvertexcount(12)]
void gs_line( 
  // expected-error@+1{{HitObjects in entry function parameters are not supported}}
  line TYPE a,
  // expected-error@+1{{HitObjects in geometry streams are not supported}}
  inout LineStream<TYPE> OutputStream0) {}


[shader("geometry")]
[maxvertexcount(12)]
void gs_line(
  // expected-error@+1{{HitObjects in entry function parameters are not supported}}
  line TYPE a,
  // expected-error@+1{{HitObjects in geometry streams are not supported}}
  inout TriangleStream<TYPE> OutputStream0) {}

[shader("domain")]
[domain("tri")]
void ds_main(
  // expected-error@+1{{HitObjects in tessellation patches are not supported}}
  OutputPatch<TYPE, 3> TrianglePatch) {}

void patch_const(
  // expected-error@+1{{HitObjects in tessellation patches are not supported}}
  InputPatch<TYPE, 3> inpatch,
  // expected-error@+1{{HitObjects in tessellation patches are not supported}}
  OutputPatch<TYPE, 3> outpatch) {}

[shader("hull")]
[domain("tri")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(32)]
[patchconstantfunc("patch_const")]
// expected-error@+1{{HitObjects in tessellation patches are not supported}}
void hs_main(InputPatch<TYPE, 3> TrianglePatch) {}

RaytracingAccelerationStructure RTAS;

struct [raypayload] DXRHitStruct {
  float4 f : write(closesthit) : read(caller);
  TYPE hit : write(closesthit) : read(caller);
};

struct [raypayload] DXRHitStructSub : DXRHitStruct {
  int3 is : write(closesthit) : read(caller);
};

template<typename T>
struct [raypayload] DXRHitTpl {
  float4 f : write(closesthit) : read(caller);
  T hit : write(closesthit) : read(caller);
};

#define RTTYPE PASTE(DXR,TYPE)

[shader("raygeneration")]
void raygen() {
  RTTYPE p = (RTTYPE)0;
  RayDesc ray = (RayDesc)0;
  TraceRay(RTAS, RAY_FLAG_NONE, 0, 0, 1, 0, ray, p); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
  CallShader(0, p); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
}

[shader("closesthit")]
void closesthit(
    // expected-error@+3{{payload parameter 'payload' must be a user-defined type composed of only numeric types}}
    // expected-note@14{{'dx::HitObject' field declared here}}
    // expected-error@+1{{HitObjects in entry function parameters are not supported}}
    inout RTTYPE payload,
    // expected-error@+3{{attributes parameter 'attribs' must be a user-defined type composed of only numeric types}}
    // expected-note@14{{'dx::HitObject' field declared here}}
    // expected-error@+1{{HitObjects in entry function parameters are not supported}}
    in RTTYPE attribs) {
  RayDesc ray;
  TraceRay( RTAS, RAY_FLAG_NONE, 0xff, 0, 1, 0, ray, payload ); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
  CallShader(0, payload); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
}

[shader("anyhit")]
void AnyHit(
    // expected-error@+3{{payload parameter 'payload' must be a user-defined type composed of only numeric types}}
    // expected-note@14{{'dx::HitObject' field declared here}}
    // expected-error@+1{{HitObjects in entry function parameters are not supported}}
    inout RTTYPE payload, 
    // expected-error@+3{{attributes parameter 'attribs' must be a user-defined type composed of only numeric types}}
    // expected-note@14{{'dx::HitObject' field declared here}}
    // expected-error@+1{{HitObjects in entry function parameters are not supported}}
    in RTTYPE attribs)
{
}

[shader("miss")]
void Miss(
    // expected-error@+3{{payload parameter 'payload' must be a user-defined type composed of only numeric types}}
    // expected-note@14{{'dx::HitObject' field declared here}}
    // expected-error@+1{{HitObjects in entry function parameters are not supported}}
    inout RTTYPE payload){
  RayDesc ray;
  TraceRay( RTAS, RAY_FLAG_NONE, 0xff, 0, 1, 0, ray, payload ); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
  CallShader(0, payload); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
}

[shader("intersection")]
void Intersection() {
  float hitT = RayTCurrent();
  RTTYPE attr = (RTTYPE)0;
  bool bReported = ReportHit(hitT, 0, attr); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
}

[shader("callable")]
void callable1(
    // expected-error@+3{{callable parameter 'p' must be a user-defined type composed of only numeric types}}
    // expected-note@14{{'dx::HitObject' field declared here}}
    // expected-error@+1{{HitObjects in entry function parameters are not supported}}
    inout RTTYPE p) { 
  CallShader(0, p); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
}

groupshared TYPE as_pld; // expected-error{{HitObjects in cbuffers or tbuffers are not supported}}

[shader("amplification")]
[numthreads(1,1,1)]
void Amp() {
  TYPE as_pld;
  DispatchMesh(1,1,1,as_pld); // expected-error{{HitObjects in user-defined struct parameter are not supported}}
}

struct NodeHitStruct {
  uint3 grid : SV_DispatchGrid;
  TYPE hit;
};

struct NodeHitStructSub : NodeHitStruct {
  int3 is;
};

template<typename T>
struct NodeHitTpl {
  uint3 grid : SV_DispatchGrid;
  T hit;
};

#define NTYPE PASTE(Node,TYPE)

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8, 1, 1)]
// Below error raised because the constructed node input record is invalid. Could be improved.
// expected-error@+1{{Broadcasting node shader 'broadcast' with NodeMaxDispatchGrid attribute must declare an input record containing a field with SV_DispatchGrid semantic}}
void broadcast(
    // expected-error@+1{{object 'dx::HitObject' may not appear in a node record}}
    DispatchNodeInputRecord<NTYPE> input,
    // expected-error@+1{{object 'dx::HitObject' may not appear in a node record}}
    NodeOutput<TYPE> output)
{
  ThreadNodeOutputRecords<TYPE> touts; // expected-error{{object 'dx::HitObject' may not appear in a node record}}
  GroupNodeOutputRecords<TYPE> gouts; // expected-error{{object 'dx::HitObject' may not appear in a node record}}
}

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(8,1,1)]
void coalesce(
    // expected-error@+1{{object 'dx::HitObject' may not appear in a node record}}
    GroupNodeInputRecords<TYPE> input) {}

[Shader("node")]
[NodeLaunch("thread")]
// expected-error@+1{{object 'dx::HitObject' may not appear in a node record}}
void threader(ThreadNodeInputRecord<TYPE> input) {}
