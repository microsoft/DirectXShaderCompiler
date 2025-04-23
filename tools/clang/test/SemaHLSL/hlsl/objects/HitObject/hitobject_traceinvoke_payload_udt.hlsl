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
  // expected-error@+1{{payload parameter 'payload_in_rg' must be a user-defined type composed of only numeric types}}
  Payload payload_in_rg;
  dx::HitObject::Invoke( dx::HitObject(), payload_in_rg );
}