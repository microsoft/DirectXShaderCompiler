// RUN: %dxc -T lib_6_6 -validator-version 1.6 %s -allow-payload-qualifiers | FileCheck %s

// CHECK: !dx.dxrPayloadAnnotations = !{{{![0-9]+}}}
// CHECK: {{![0-9]+}} = !{i32 0, %struct.Payload undef, {{![0-9]+}}}
// CHECK: {{![0-9]+}} = !{{{![0-9]+}}}
// CHECK: {{![0-9]+}} = !{i32 0, !"a", i32 1, {{![0-9]+}}, {{![0-9]+}}, {{![0-9]+}}, {{![0-9]+}}}
// CHECK: {{![0-9]+}} = !{!"trace", i32 0}
// CHECK: {{![0-9]+}} = !{!"closesthit", i32 0}
// CHECK: {{![0-9]+}} = !{!"anyhit", i32 1}
// CHECK: {{![0-9]+}} = !{!"miss", i32 2}

// Check if metadata is correctly emitted into the dx.dxrPayloadAnnotations metadata and not stripped.
// trace = 0 => inout
// closesthit = 0 => inout
// anyhithit = 1 => in
// miss = 2 => out

struct Payload
{
    int a      : in(trace, closesthit, anyhit) : out(trace, miss, closesthit);
};

[shader("miss")]
void Miss( inout Payload payload ) {}