// RUN: %dxc -T lib_6_9 %s

// COM: No FileCheck required: Error raised by ASAN-enabled DXC builds where underlying bug #7104 is not fixed.
// COM: This checks that the backing memory of the shader stages vector does not migrate when more than four (it's preallocated size) shader stages are added.

struct [raypayload] Payload
{
    int a      : read(anyhit,caller,miss,closesthit,closesthit) : write(anyhit,caller,miss,closesthit,caller);
};

struct Attribs
{
    float2 barys;
};

[shader("closesthit")]
void ClosestHitInOut( inout Payload payload, in Attribs attribs )
{
  if (payload.a == 1)
    payload.a = 2;
}