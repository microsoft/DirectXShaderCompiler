// RUN: %if dxil-1-9 %{ %dxc -verify -T lib_6_9 %s %}
// ==================================================================
// Test mesh nodes with a few failing attrib states
// ==================================================================

[Shader("node")]
[NodeLaunch("broadcasting")] // expected-note {{Launch type defined here}}
[NumThreads(1,1,1)]
[NodeDispatchGrid(4,1,1)]
[NodeMaxInputRecordsPerGraphEntryRecord(11, false)] // expected-error {{'nodemaxinputrecordspergraphentryrecord' may only be used with mesh nodes}}
void broadcasting_node() {}

[Shader("node")]
[NodeLaunch("coalescing")] // expected-note {{Launch type defined here}}
[NumThreads(1,1,1)]
[NodeMaxInputRecordsPerGraphEntryRecord(11, false)] // expected-error {{'nodemaxinputrecordspergraphentryrecord' may only be used with mesh nodes}}
void coalescing_node() {}

[Shader("node")]
[NodeLaunch("thread")] // expected-note {{Launch type defined here}}
[NumThreads(1,1,1)]
[NodeMaxInputRecordsPerGraphEntryRecord(11, false)] // expected-error {{'nodemaxinputrecordspergraphentryrecord' may only be used with mesh nodes}}
void thread_node() {}

[Shader("node")]
[NodeLaunch("mesh")]
[OutputTopology("line")]
[NumThreads(4,3,22)] // expected-warning {{Group size of 264 (4 * 3 * 22) is outside of valid range [1..128] - attribute will be ignored}}
[NodeDispatchGrid(4,1,1)]
void mesh_node_numth_count() {} // expected-error {{node entry point must have a valid numthreads attribute}}

[Shader("node")]
[NodeLaunch("mesh")]
[OutputTopology("line")]
[NumThreads(129,1, 1)] // expected-warning {{Thread Group X size of 129 is outside of valid range [1..128] - attribute will be ignored}}
// expected-warning@-1 {{Group size of 129 (129 * 1 * 1) is outside of valid range [1..128] - attribute will be ignored}}
[NodeDispatchGrid(4,1,1)]
void mesh_node_numth_x() {} // expected-error {{node entry point must have a valid numthreads attribute}}

[Shader("node")]
[NodeLaunch("mesh")]
[OutputTopology("line")]
[NumThreads(1,129,1)] // expected-warning {{Thread Group Y size of 129 is outside of valid range [1..128] - attribute will be ignored}}
// expected-warning@-1 {{Group size of 129 (1 * 129 * 1) is outside of valid range [1..128] - attribute will be ignored}}
[NodeDispatchGrid(4,1,1)]
void mesh_node_numth_y() {} // expected-error {{node entry point must have a valid numthreads attribute}}

[Shader("node")]
[NodeLaunch("mesh")]
[OutputTopology("line")]
[NumThreads(1,1,129)] // expected-warning {{Thread Group Z size of 129 is outside of valid range [1..128] - attribute will be ignored}}
// expected-warning@-1 {{Group size of 129 (1 * 1 * 129) is outside of valid range [1..128] - attribute will be ignored}}
[NodeDispatchGrid(4,1,1)]
void mesh_node_numth_z() {} // expected-error {{node entry point must have a valid numthreads attribute}}

[Shader("compute")]
[NumThreads(1,1,1)]
[OutputTopology("foo")]  // expected-error {{attribute 'OutputTopology' must have one of these values: point,line,triangle,triangle_cw,triangle_ccw}}
void compute_node_topology() {}

[Shader("compute")]
[NumThreads(1,1,1)]
[OutputTopology("triangle")]
[NodeMaxInputRecordsPerGraphEntryRecord(7, false)] // expected-error {{attribute nodemaxinputrecordspergraphentryrecord only allowed on node shaders}}
void compute_node_maxrecs() {}
