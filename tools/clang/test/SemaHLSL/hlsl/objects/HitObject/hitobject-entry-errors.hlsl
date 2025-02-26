// RUN: %dxc -T lib_6_9 %s -verify

struct [raypayload] Payload
{
    float elem
          : write(caller,anyhit,closesthit,miss)
          : read(caller,anyhit,closesthit,miss);
};

struct Attribs { float2 barys; };

void UseHitObject() {
  HitObject hit;
}

// expected-note@+3{{entry function defined here}}
[shader("compute")]
[numthreads(4,4,4)]
void mainHitCS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
// expected-error@-7{{Shader kind 'compute' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
  UseHitObject();
}

// expected-note@+2{{entry function defined here}}
[shader("callable")]
void mainHitCALL(inout Attribs attrs) {
// expected-error@-14{{Shader kind 'callable' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
  UseHitObject();
}

// expected-note@+2{{entry function defined here}}
[shader("intersection")]
void mainHitIS() {
// expected-error@-21{{Shader kind 'intersection' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
  UseHitObject();
}

// expected-note@+2{{entry function defined here}}
[shader("anyhit")]
void mainHitAH(inout Payload pld, in Attribs attrs) {
// expected-error@-28{{Shader kind 'anyhit' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
  UseHitObject();
}

[shader("raygeneration")]
void mainHitRG() {
  UseHitObject();
}

[shader("closesthit")]
void mainHitCH(inout Payload pld, in Attribs attrs) {
  UseHitObject();
}

[shader("miss")]
void mainHitMS(inout Payload pld) {
  UseHitObject();
}
