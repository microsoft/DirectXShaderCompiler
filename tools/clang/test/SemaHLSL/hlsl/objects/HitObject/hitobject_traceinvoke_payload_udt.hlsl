// RUN: %dxc -T lib_6_9 %s -verify

struct
[raypayload]
Payload
{
    int a : read(caller, closesthit, miss) : write(caller, closesthit, miss);
    dx::HitObject hit;
};

struct Attribs
{
    float2 barys;
};

[shader("raygeneration")]
void RayGen()
{
  // expected-error@+2{{object ''dx::HitObject'' is not allowed in payload parameters}}
  // expected-note@8{{'dx::HitObject' field declared here}}
  Payload payload_in_rg;
  dx::HitObject::Invoke( dx::HitObject(), payload_in_rg );
}