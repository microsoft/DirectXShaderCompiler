// RUN: %dxc -T lib_6_3 -HV 202x %s -verify

void InvalidArgumentCount() {
  hlsl::sin(); // expected-error{{no matching function for call to 'sin'}} expected-note{{intrinsic function does not accept 0 arguments}}
  hlsl::sin(1.0f, 2.0f); // expected-error{{no matching function for call to 'sin'}} expected-note{{intrinsic function does not accept 2 arguments}}
}

void InvalidArgumentCountAfterSynthesis(float x) {
  (void)hlsl::sin(x);
  hlsl::sin(); // expected-error{{no matching function for call to 'sin'}} expected-note{{intrinsic function does not accept 0 arguments}}
  hlsl::sin(x, x); // expected-error{{no matching function for call to 'sin'}} expected-note{{intrinsic function does not accept 2 arguments}}
}

void InvalidImportedArgumentCount() {
  using hlsl::sin;
  sin(); // expected-error{{no matching function for call to 'sin'}} expected-note{{intrinsic function does not accept 0 arguments}}
  sin(1.0f, 2.0f); // expected-error{{no matching function for call to 'sin'}} expected-note{{intrinsic function does not accept 2 arguments}}
}

void InvalidUsingDirectiveArgumentCount() {
  using namespace hlsl;
  cos(); // expected-error{{no matching function for call to 'cos'}} expected-note{{intrinsic function does not accept 0 arguments}}
  cos(1.0f, 2.0f); // expected-error{{no matching function for call to 'cos'}} expected-note{{intrinsic function does not accept 2 arguments}}
}

void InvalidRayDesc() {
  hlsl::TraceRay(1, 2, 3, 4, 5, 6, 7, 8); // expected-error{{no matching function for call to 'TraceRay'}} expected-error{{Argument type must be struct RayDesc}}
}

void InvalidPayload(RayDesc ray) {
  hlsl::TraceRay(1, 2, 3, 4, 5, 6, ray, 8); // expected-error{{no matching function for call to 'TraceRay'}} expected-error{{User defined type intrinsic arg must be struct}}
}
