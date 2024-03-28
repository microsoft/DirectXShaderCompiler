// RUN: %if dxil-1-9 %{ %dxc -T lib_6_9 -verify %s | FileCheck %s %}

// Test that invalid mesh node input parameters fail with appropriate diagnostics

struct RECORD {
  uint3 gtid;
};

[Shader("node")]
[numthreads(4,4,4)]
[NodeDispatchGrid(4,4,4)]
[NodeLaunch("mesh")] // expected-note {{Launch type defined here}}
void node01_rw(RWDispatchNodeInputRecord<RECORD> input, // expected-error {{'RWDispatchNodeInputRecord' may not be used with mesh nodes (only DispatchNodeInputRecord)}}
 uint3 GTID : SV_GroupThreadID ) {
  input.Get().gtid = GTID;
}

[Shader("node")]
[numthreads(4,4,4)]
[NodeMaxDispatchGrid(4,4,4)]
[NodeLaunch("mesh")] // expected-note {{Launch type defined here}}
void node02_maxdisp(DispatchNodeInputRecord<RECORD> input, // expected-error {{Broadcasting/Mesh node shader 'node02_maxdisp' with NodeMaxDispatchGrid attribute must declare an input record containing a field with SV_DispatchGrid semantic}}
 uint3 GTID : SV_GroupThreadID ) {
}
