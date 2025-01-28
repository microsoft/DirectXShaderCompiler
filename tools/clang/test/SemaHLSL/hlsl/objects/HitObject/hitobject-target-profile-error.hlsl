// RUN: %dxc -T lib_6_8 %s -verify

struct [raypayload] Payload {
  float3 dummy : read(closesthit) : write(caller, anyhit);
};

[shader("raygeneration")] void main() {
  Payload pld;
  pld.dummy = float3(7.0, 8.0, 9.0);

  // expected-error@+2{{Shader execution reordering requires target profile lib_6_9+ (was lib_6_8)}}
  HitObject hit;
  HitObject::Invoke(hit, pld);
}
