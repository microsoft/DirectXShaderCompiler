// RUN: %dxc -T lib_6_x %s -verify

// lib_6_x is an offline-linking-only target that defers final shader model
// availability checks to link time. Declaring node shaders of any launch
// type, and using node record types (including in exported library helper
// functions), must not produce any diagnostics when targeting lib_6_x, even
// though these are obsoleted at shader model 6.10.

// expected-no-diagnostics

struct RECORD
{
  uint a;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1,1,1)]
[NumThreads(1,1,1)]
void broadcasting_node(DispatchNodeInputRecord<RECORD> input) {}

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1,1,1)]
void coalescing_node() {}

[Shader("node")]
[NodeLaunch("thread")]
void thread_node() {}

export void HelperUsesNodeOutput(NodeOutput<RECORD> output) {}
