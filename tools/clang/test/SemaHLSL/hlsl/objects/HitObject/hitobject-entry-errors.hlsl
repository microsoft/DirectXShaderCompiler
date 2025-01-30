// RUN: %dxc -T lib_6_9 %s -verify

struct [raypayload] Payload
{
    float elem
          : write(caller,anyhit,closesthit,miss)
          : read(caller,anyhit,closesthit,miss);
};

struct Attribs { float2 barys; };

void UseHitObject() {
// expected-error@+4{{Shader kind 'compute' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
// expected-error@+3{{Shader kind 'intersection' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
// expected-error@+2{{Shader kind 'anyhit' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
// expected-error@+1{{Shader kind 'callable' incompatible with shader execution reordering (has to be raygeneration, closesthit or miss)}}
  HitObject hit;
}

// expected-note@+3{{entry function defined here}}
[shader("compute")]
[numthreads(4,4,4)]
void mainHitCS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  UseHitObject();
}

[shader("raygeneration")]
void mainHitRG() {
  UseHitObject();
}

// expected-note@+2{{entry function defined here}}
[shader("callable")]
void mainHitCALL(inout Attribs attrs) {
  UseHitObject();
}
// expected-note@+2{{entry function defined here}}
[shader("intersection")]
void mainHitIS() {
  UseHitObject();
}

// expected-note@+2{{entry function defined here}}
[shader("anyhit")]
void mainHitAH(inout Payload pld, in Attribs attrs) {
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
