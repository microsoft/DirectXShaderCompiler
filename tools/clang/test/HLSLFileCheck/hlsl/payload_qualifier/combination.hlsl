// RUN: %dxc -T lib_6_6 main %s -allow-payload-qualifiers | FileCheck %s

// CHECK: payload access qualifiers are not supported on struct types.
// CHECK: expected identifier
// CHECK: field is qualified 'in' for trace but a valid shader stage is missing. Supported shader stages are closesthit, miss or anyhit
// CHECK: field is qualified 'in' for shader stage 'anyhit' but trace is missing
// CHECK: field is qualified 'out' for shader stage 'closesthit' but trace is missing
// CHECK: payload access qualifiers are only defined for raytracing shader stages closesthit, miss, anyhit and for special keyword: trace. 'lollipop' is not supported
// CHECK: payload access qualifier 'out' has already been defined
// CHECK: field is marked 'out' for 'anyhit' but is not marked 'in' for closesthit or miss or trace
// CHECK: field is marked 'in' for 'miss' but is not marked 'out' for anyhit or trace
// CHECK: field is qualified 'out' for trace but a valid shader stage is missing. Supported shader stages are closesthit, miss or anyhit

// Check for valid combinations of in/out qualifiers.

struct S1 {
    int foo;
};

struct [[payload]] Payload
{
    S1 bar       : in(trace, closesthit);
    int a        : in();
    int b        : in(trace);
    int c        : in(anyhit);
    int d        : out(closesthit);
    int e        : in(trace, lollipop);
    int f        : out(trace, closesthit) : out(trace);
    int g        : out(anyhit) : in(closesthit);
    int h        : out(anyhit);
    int i        : in(miss);
    int j        : out(trace);
};