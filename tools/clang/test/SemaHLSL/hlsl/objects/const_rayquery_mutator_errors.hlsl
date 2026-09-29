// RUN: %dxc -T lib_6_5 -HV 202x -verify %s

// Verify that RayQuery mutators (TraceRayInline, Proceed, Abort, and the
// Commit* helpers) cannot be invoked on a const-qualified RayQuery, while
// the read-only accessors (CommittedStatus, CandidateType, ...) remain
// callable.

RaytracingAccelerationStructure RTAS : register(t0);

void use_const(const RayQuery<RAY_FLAG_NONE, 0xff> cq) {
  RayDesc desc = (RayDesc)0;
  // expected-error@+2{{no matching member function for call to 'TraceRayInline'}}
  // expected-note@+1 1+ {{but method is not marked const}}
  cq.TraceRayInline(RTAS, RAY_FLAG_NONE, 0xff, desc);
  // expected-error@+2{{no matching member function for call to 'Proceed'}}
  // expected-note@+1 1+ {{but method is not marked const}}
  bool ok = cq.Proceed();
  // expected-error@+2{{no matching member function for call to 'Abort'}}
  // expected-note@+1 1+ {{but method is not marked const}}
  cq.Abort();
  // expected-error@+2{{no matching member function for call to 'CommitNonOpaqueTriangleHit'}}
  // expected-note@+1 1+ {{but method is not marked const}}
  cq.CommitNonOpaqueTriangleHit();
  // expected-error@+2{{no matching member function for call to 'CommitProceduralPrimitiveHit'}}
  // expected-note@+1 1+ {{but method is not marked const}}
  cq.CommitProceduralPrimitiveHit(1.0f);

  // Const accessors are fine.
  uint s = cq.CommittedStatus();
  uint t = cq.CandidateType();
  (void)s; (void)t;
}

[shader("raygeneration")]
void main() {
  RayQuery<RAY_FLAG_NONE, 0xff> q;
  use_const(q);
}
