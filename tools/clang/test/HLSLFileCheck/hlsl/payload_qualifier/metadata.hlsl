// RUN: %dxc -T lib_6_6 -validator-version 1.6 %s -allow-payload-qualifiers | FileCheck %s

// CHECK: !dx.dxrPayloadAnnotations = !{{{![0-9]+}}}
// CHECK: {{![0-9]+}} = !{i32 0, %struct.Payload undef, {{![0-9]+}}}
// CHECK: {{![0-9]+}} = !{{{![0-9]+}}}
// CHECK: {{![0-9]+}} = !{i32 0, !"a", i32 1, i32 4659}

struct [payload] Payload
{
    int a      : in(trace, closesthit, anyhit) : out(trace, miss, closesthit);
};

[shader("miss")]
void Miss( inout Payload payload ) {
    payload.a = 42;
}