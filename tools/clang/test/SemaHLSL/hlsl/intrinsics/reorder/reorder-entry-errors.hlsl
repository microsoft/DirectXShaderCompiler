// RUN: %dxc -T lib_6_9 %s -verify

struct [raypayload] Payload
{
    float elem
          : write(caller,closesthit,anyhit,closesthit,miss)
          : read(caller,closesthit,anyhit,closesthit,miss);
};

struct Attribs { float2 barys; };
void CallReorder()
{
// expected-error@+6{{Shader kind 'compute' incompatible with the ReorderThread intrinsic (only in raygeneration)}}
// expected-error@+5{{Shader kind 'callable' incompatible with the ReorderThread intrinsic (only in raygeneration)}}
// expected-error@+4{{Shader kind 'intersection' incompatible with the ReorderThread intrinsic (only in raygeneration)}}
// expected-error@+3{{Shader kind 'anyhit' incompatible with the ReorderThread intrinsic (only in raygeneration)}}
// expected-error@+2{{Shader kind 'closesthit' incompatible with the ReorderThread intrinsic (only in raygeneration)}}
// expected-error@+1{{Shader kind 'miss' incompatible with the ReorderThread intrinsic (only in raygeneration)}}
  ReorderThread(0,0);
}

// expected-note@+3{{entry function defined here}}
[shader("compute")]
[numthreads(4,4,4)]
void mainReorderCS(uint ix : SV_GroupIndex, uint3 id : SV_GroupThreadID) {
  CallReorder();
}

[shader("raygeneration")]
void mainReorderRG() {
  CallReorder();
}

// expected-note@+2{{entry function defined here}}
[shader("callable")]
void mainReorderCALL(inout Attribs attrs) {
  CallReorder();
}

// expected-note@+2{{entry function defined here}}
[shader("intersection")]
void mainReorderIS() {
  CallReorder();
}

// expected-note@+2{{entry function defined here}}
[shader("anyhit")]
void mainReorderAH(inout Payload pld, in Attribs attrs) {
  CallReorder();
}

// expected-note@+2{{entry function defined here}}
[shader("closesthit")]
void mainReorderCH(inout Payload pld, in Attribs attrs) {
  CallReorder();
}

// expected-note@+2{{entry function defined here}}
[shader("miss")]
void mainReorderMS(inout Payload pld) {
  CallReorder();
}

