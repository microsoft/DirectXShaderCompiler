// RUN: %dxc -T lib_6_3 -HV 202x %s -verify

void InvalidRayDesc() {
  hlsl::TraceRay(1, 2, 3, 4, 5, 6, 7, 8); // expected-error{{no matching function for call to 'TraceRay'}} expected-error{{Argument type must be struct RayDesc}}
}

void InvalidPayload(RayDesc ray) {
  hlsl::TraceRay(1, 2, 3, 4, 5, 6, ray, 8); // expected-error{{no matching function for call to 'TraceRay'}} expected-error{{User defined type intrinsic arg must be struct}}
}
