// RUN: %dxc -T lib_6_6 main %s -enable-payload-qualifiers | FileCheck %s

// CHECK: error: field 'a' is not modifiable because it is not qualified 'write' for shader stage 'closesthit'
// CHECK: error: field 'b' is not readable because it is not qualified 'read' for shader stage 'closesthit'
// CHECK: error: field 'arr' is not readable because it is not qualified 'read' for shader stage 'closesthit'
// CHECK: error: field 'arr2' is not modifiable because it is not qualified 'write' for shader stage 'closesthit'

// CHECK: error: field 'a' is written in function 'bar', but is not qualified 'write' for shader stage 'closesthit'
// CHECK: error: field 'b' is written in function 'bar', but is not qualified 'write' for shader stage 'miss'


struct [payload] Payload
{
    int a      : read(caller, closesthit) : write(caller, miss);
    int b      : write(caller, closesthit) : read(caller, miss);
    int arr[33]  : write(closesthit) : read(caller);
    int arr2[33] : read(closesthit) : write(caller);
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