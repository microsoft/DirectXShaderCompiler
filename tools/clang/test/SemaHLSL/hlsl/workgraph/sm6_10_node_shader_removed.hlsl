// RUN: %dxc -T lib_6_10 %s -verify

// Work Graphs (node shaders) were obsoleted in shader model 6.10. Declaring a
// node shader of any launch type when targeting shader model 6.10 or above
// must be an error, regardless of whether any node record types are used.
// Using a node record type must also be an error, independent of whether the
// entry point declaring it is itself a node shader.

struct RECORD
{
  uint a;
};

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1,1,1)]
[NumThreads(1,1,1)]
// expected-error@+1{{node shaders and other Work Graphs functionality are not supported when targeting shader model lib_6_10}}
void broadcasting_node() {}

[Shader("node")]
[NodeLaunch("coalescing")]
[NumThreads(1,1,1)]
// expected-error@+1{{node shaders and other Work Graphs functionality are not supported when targeting shader model lib_6_10}}
void coalescing_node() {}

[Shader("node")]
[NodeLaunch("thread")]
// expected-error@+1{{node shaders and other Work Graphs functionality are not supported when targeting shader model lib_6_10}}
void thread_node() {}

// Using a node record type is independently an error, on top of the node
// shader declaration error above.
[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1,1,1)]
[NumThreads(1,1,1)]
// expected-error@+2{{node shaders and other Work Graphs functionality are not supported when targeting shader model lib_6_10}}
// expected-error@+1{{built-in type 'DispatchNodeInputRecord<RECORD>' is unavailable in shader model lib_6_10; it was removed in shader model 6.10}}
void node_with_input(DispatchNodeInputRecord<RECORD> input) {}

// A node record type used outside of a node shader entry point (e.g. in a
// library helper) is still an error at shader model 6.10. The helper must be
// exported so that it is checked even though nothing calls it.
// expected-error@+1{{built-in type 'NodeOutput<RECORD>' is unavailable in shader model lib_6_10; it was removed in shader model 6.10}}
export void HelperUsesNodeOutput(NodeOutput<RECORD> output) {}
