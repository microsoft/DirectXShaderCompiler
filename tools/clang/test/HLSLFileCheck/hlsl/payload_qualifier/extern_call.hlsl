// RUN: %dxc -T lib_6_6 %s -allow-payload-qualifiers | FileCheck %s

// CHECK: warning: passing a qualified payload to an extern function can cause undefined behavior if payload qualifiers mismatch
// CHECK: error: passing a pure 'in' payload to an extern function as 'out' parameter
// CHECK: error: passing a pure 'out' payload to an extern function as 'in' parameter

struct PayloadInputOnly
{
    int a      : in(trace, closesthit);
};

struct PayloadOutputOnly
{
    int a      : out(trace, closesthit);
};

struct PayloadInOut
{
    int a      : in(trace, closesthit) : out(trace, closesthit);
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