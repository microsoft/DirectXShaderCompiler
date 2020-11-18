// RUN: %dxc -T lib_6_6 main %s -allow-payload-qualifiers | FileCheck %s

// CHECK: field 'a' is not modifiable because it is not qualified 'out' for shader stage 'closesthit'
// CHECK: field 'b' is not readable because it is not qualified 'in' for shader stage 'closesthit'
// CHECK: field 'arr' is not readable because it is not qualified 'in' for shader stage 'closesthit'
// CHECK: field 'arr2' is not modifiable because it is not qualified 'out' for shader stage 'closesthit'

// CHECK: field 'a' is not modifiable in function 'bar'. 'bar' is called from shader stage 'closesthit' but 'a' is not qualified 'out' for this stage
// CHECK: field 'b' is not modifiable in function 'bar'. 'bar' is called from shader stage 'miss' but 'b' is not qualified 'out' for this stage

struct [[payload]] Payload
{
    int a      : in(trace, closesthit) : out(trace, miss);
    int b      : out(trace, closesthit) : in(trace, miss);
    int arr[33]  : out(trace, closesthit);
    int arr2[33] : in(trace, closesthit);
};

struct Attribs
{
    float2 barys;
};

void bar(inout Payload payload)
{
    payload.b = 43;
    payload.a = 44;
}

void foo(inout Payload payload)
{
    bar(payload);
}


[shader("closesthit")]
void ClosestHit( inout Payload payload, in Attribs attribs )
{
    payload.a = 42;
    int var1 = payload.b * 2;
    int var2 = payload.arr[23];
    payload.arr2[2] = 42;

    foo(payload);
}

[shader("miss")]
void Miss( inout Payload payload )
{
    foo(payload);
}