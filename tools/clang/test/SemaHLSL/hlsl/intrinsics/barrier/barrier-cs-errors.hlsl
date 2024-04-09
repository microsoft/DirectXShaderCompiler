// RUN: %dxc -Tlib_6_8 -verify %s
// RUN: %dxc -Tcs_6_8 -verify %s
// REQUIRES: dxil-1-8

// Test the ordinary shader model case with no visible group.

struct RECORD { uint a; };
RWBuffer<uint> buf0;
static uint i = 7;

// Node barriers not supported in non-node shaders.
[noinline] export
void NodeBarriers() {
  // expected-error@+1{{NODE_INPUT_MEMORY or NODE_OUTPUT_MEMORY may only be specified for Barrier operation in a node shader}}
  Barrier(NODE_INPUT_MEMORY, 0);
  // expected-error@+1{{NODE_INPUT_MEMORY or NODE_OUTPUT_MEMORY may only be specified for Barrier operation in a node shader}}
  Barrier(NODE_INPUT_MEMORY, DEVICE_SCOPE);

  // expected-error@+1{{NODE_INPUT_MEMORY or NODE_OUTPUT_MEMORY may only be specified for Barrier operation in a node shader}}
  Barrier(NODE_OUTPUT_MEMORY, 0);
  // expected-error@+1{{NODE_INPUT_MEMORY or NODE_OUTPUT_MEMORY may only be specified for Barrier operation in a node shader}}
  Barrier(NODE_OUTPUT_MEMORY, DEVICE_SCOPE);
}

// expected-note@+6{{entry function defined here}}
// expected-note@+5{{entry function defined here}}
// expected-note@+4{{entry function defined here}}
// expected-note@+3{{entry function defined here}}
[Shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  NodeBarriers();

  // expected-error@+1{{invalid MemoryTypeFlags for Barrier operation; expected 0, ALL_MEMORY, or some combination of UAV_MEMORY, GROUP_SHARED_MEMORY, NODE_INPUT_MEMORY, NODE_OUTPUT_MEMORY flags}}
  Barrier(16, 0);
  // expected-error@+1{{invalid MemoryTypeFlags for Barrier operation; expected 0, ALL_MEMORY, or some combination of UAV_MEMORY, GROUP_SHARED_MEMORY, NODE_INPUT_MEMORY, NODE_OUTPUT_MEMORY flags}}
  Barrier(-1, 0);
  // expected-error@+1{{invalid SemanticFlags for Barrier operation; expected 0 or some combination of GROUP_SYNC, GROUP_SCOPE, DEVICE_SCOPE flags}}
  Barrier(0, 8);
  // expected-error@+1{{invalid SemanticFlags for Barrier operation; expected 0 or some combination of GROUP_SYNC, GROUP_SCOPE, DEVICE_SCOPE flags}}
  Barrier(0, -1);
}
