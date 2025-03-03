// RUN: %dxc  -DTYPE=float -DNUM=7 -T ps_6_9 -verify %s

struct [raypayload] LongVec {
  float4 f : write(closesthit) : read(caller);
  vector<TYPE,NUM> vec : write(closesthit) : read(caller);
};

struct LongVecParm {
  float f;
  float4 tar2 : SV_Target2;
  vector<TYPE,NUM> vec;
};

vector<TYPE, NUM> global_vec; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}

vector<TYPE, NUM> global_vec_arr[10]; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}

LongVec global_vec_rec; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}

cbuffer BadBuffy {
  vector<TYPE, NUM> cb_vec; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}
  vector<TYPE, NUM> cb_vec_arr[10]; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}
  LongVec cb_vec_rec; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}
};

tbuffer BadTuffy {
  vector<TYPE, NUM> tb_vec; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}
  vector<TYPE, NUM> tb_vec_arr[10]; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}
  LongVec tb_vec_rec; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}
};

ConstantBuffer< LongVec > const_buf; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}
TextureBuffer< LongVec > tex_buf; // expected-error{{Vectors of over 4 elements in cbuffers are not supported}}

vector<TYPE, 5> main( // expected-error{{Vectors of over 4 elements in entry function return type are not supported}}
                     vector<TYPE, NUM> vec : V, // expected-error{{Vectors of over 4 elements in entry function parameters are not supported}}
                     LongVecParm parm : P) : SV_Target { // expected-error{{Vectors of over 4 elements in entry function parameters are not supported}}
  parm.f = vec; // expected-warning {{implicit truncation of vector type}}
  parm.tar2 = vec; // expected-warning {{implicit truncation of vector type}}
  return vec; // expected-warning {{implicit truncation of vector type}}
}

[shader("domain")]
[domain("tri")]
void ds_main(OutputPatch<LongVec, 3> TrianglePatch) {} // expected-error{{Vectors of over 4 elements in tessellation patches are not supported}}

void PatchConstantFunction(InputPatch<LongVec, 3> inpatch, // expected-error{{Vectors of over 4 elements in tessellation patches are not supported}}
			   OutputPatch<LongVec, 3> outpatch) {} // expected-error{{Vectors of over 4 elements in tessellation patches are not supported}}


[shader("hull")]
[domain("tri")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(32)]
[patchconstantfunc("PatchConstantFunction")]
void hs_main(InputPatch<LongVec, 3> TrianglePatch) {} // expected-error{{Vectors of over 4 elements in tessellation patches are not supported}}

RaytracingAccelerationStructure RTAS;

[shader("raygeneration")]
void raygen() {
  LongVec p = (LongVec)0;
  RayDesc ray = (RayDesc)0;
  TraceRay(RTAS, RAY_FLAG_NONE, 0, 0, 1, 0, ray, p); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
  CallShader(0, p); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
}

[shader("closesthit")]
void closesthit(inout LongVec payload, // expected-error{{Vectors of over 4 elements in entry function parameters are not supported}}
		in LongVec attribs ) { // expected-error{{Vectors of over 4 elements in entry function parameters are not supported}}
  RayDesc ray;
  TraceRay( RTAS, RAY_FLAG_NONE, 0xff, 0, 1, 0, ray, payload ); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
  CallShader(0, payload); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
}

[shader("anyhit")]
void AnyHit( inout LongVec payload, // expected-error{{Vectors of over 4 elements in entry function parameters are not supported}}
	      in LongVec attribs  ) // expected-error{{Vectors of over 4 elements in entry function parameters are not supported}}
{
}

[shader("miss")]
void Miss(inout LongVec payload){ // expected-error{{Vectors of over 4 elements in entry function parameters are not supported}}
  RayDesc ray;
  TraceRay( RTAS, RAY_FLAG_NONE, 0xff, 0, 1, 0, ray, payload ); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
  CallShader(0, payload); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
}

[shader("intersection")]
void Intersection() {
  float hitT = RayTCurrent();
  LongVec attr = (LongVec)0;
  bool bReported = ReportHit(hitT, 0, attr); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
}

[shader("callable")]
void callable1(inout LongVec p) { // expected-error{{Vectors of over 4 elements in entry function parameters are not supported}}
  CallShader(0, p); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
}

groupshared LongVec as_pld;

[shader("amplification")]
[numthreads(1,1,1)]
void Amp() {
  DispatchMesh(1,1,1,as_pld); // expected-error{{Vectors of over 4 elements in user-defined struct parameter are not supported}}
}

struct LongVecRec {
  uint3 grid : SV_DispatchGrid;
  vector<TYPE,NUM> vec;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NumThreads(8,1,1)]
[NodeMaxDispatchGrid(8,1,1)]
void broadcast(DispatchNodeInputRecord<LongVecRec> input,  // expected-error{{Vectors of over 4 elements in node records are not supported}}
                NodeOutput<LongVec> output) // expected-error{{Vectors of over 4 elements in node records are not supported}}
{
  ThreadNodeOutputRecords<LongVec> touts; // expected-error{{Vectors of over 4 elements in node records are not supported}}
  GroupNodeOutputRecords<LongVec> gouts; // expected-error{{Vectors of over 4 elements in node records are not supported}}
}

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(8,1,1)]
void coalesce(GroupNodeInputRecords<LongVec> input) {} // expected-error{{Vectors of over 4 elements in node records are not supported}}

[Shader("node")]
[NodeLaunch("thread")]
void threader(ThreadNodeInputRecord<LongVec> input) {} // expected-error{{Vectors of over 4 elements in node records are not supported}}
