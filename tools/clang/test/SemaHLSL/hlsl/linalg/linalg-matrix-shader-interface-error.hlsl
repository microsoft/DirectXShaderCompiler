// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -Wno-hlsl-availability -verify %s

// LinAlg matrices are opaque, thread-local values and cannot cross shader
// interfaces such as entry signatures, ray payloads, or hit attributes.

struct RawHandleState {
  __builtin_LinAlgMatrix Handle; // expected-note 6 {{field declared here}}
};

struct Numeric {
  float4 V;
};

// expected-error@+2 {{object '__builtin_LinAlgMatrix' is not allowed in entry function parameters}}
[shader("compute")] [numthreads(1, 1, 1)]
void CSParam(RawHandleState S : A) {}

// expected-error@+2 {{object '__builtin_LinAlgMatrix' is not allowed in entry function return type}}
[shader("pixel")]
RawHandleState PSReturn() : SV_Target {
  RawHandleState S;
  return S;
}

RaytracingAccelerationStructure AS;

[shader("raygeneration")]
void RGTrace() {
  RawHandleState Payload;
  RayDesc Ray = (RayDesc)0;
  // expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in user-defined struct parameter}}
  TraceRay(AS, 0, 0xff, 0, 1, 0, Ray, Payload);

  Numeric NumericPayload;
  TraceRay(AS, 0, 0xff, 0, 1, 0, Ray, NumericPayload);
}

// expected-error@+3 {{object '__builtin_LinAlgMatrix' is not allowed in entry function parameters}}
// expected-error@+2 {{payload parameter 'Payload' must be a user-defined type composed of only numeric types}}
[shader("closesthit")]
void CHPayload(inout RawHandleState Payload,
               BuiltInTriangleIntersectionAttributes Attrs) {}

[shader("intersection")]
void ISReportHit() {
  RawHandleState Attrs;
  // expected-error@+1 {{object '__builtin_LinAlgMatrix' is not allowed in attributes}}
  ReportHit(0.0, 0, Attrs);
}

// expected-error@+2 {{object '__builtin_LinAlgMatrix' is not allowed in entry function parameters}}
[shader("compute")] [numthreads(1, 1, 1)]
void CSArrayParam(RawHandleState S[2] : B) {}

// Non-entry functions may take and return LinAlg matrices.
RawHandleState Passthrough(RawHandleState S) { return S; }
