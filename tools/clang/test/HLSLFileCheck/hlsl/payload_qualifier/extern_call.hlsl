// RUN: %dxc -T lib_6_6 %s -allow-payload-qualifiers | FileCheck %s

// CHECK: warning: passing a qualified payload to an extern function can cause undefined behavior if payload qualifiers mismatch
// CHECK: error: passing a pure 'read' payload to an extern function as 'out' parameter
// CHECK: error: passing a pure 'write' payload to an extern function as 'in' parameter

struct [payload] PayloadInputOnly
{
    int a      : read(closesthit) : write(caller);
};

struct [payload] PayloadOutputOnly
{
    int a      : write(closesthit) : read(caller);
};

struct [payload] PayloadInOut
{
    int a      : read(caller, closesthit) : write(caller, closesthit);
};

struct Attribs
{
    float2 barys;
};

void foo( inout PayloadInOut );
void bar_in( in PayloadInputOnly );
void bar_out( in PayloadOutputOnly );
void baz_in( out PayloadInputOnly );
void baz_out( out PayloadOutputOnly );

[shader("closesthit")]
void ClosestHitInOut( inout PayloadInOut payload, in Attribs attribs )
{
    foo(payload); // warn about passing an inout payload to an external function
}

[shader("closesthit")]
void ClosestHitIn( inout PayloadInputOnly payload, in Attribs attribs )
{
    bar_in(payload);
    baz_in(payload); // generate an error about incompatbile qualification
}

[shader("closesthit")]
void ClosestHitOut( inout PayloadOutputOnly payload, in Attribs attribs )
{
    bar_out(payload); // generate an error about incompatbile qualification
    baz_out(payload);
}