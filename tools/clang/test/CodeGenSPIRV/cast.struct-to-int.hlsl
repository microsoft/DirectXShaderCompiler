// RUN: %dxc -T cs_6_0 -HV 2021 -E main

struct ColorRGBA { 
    uint R : 8;
    uint G : 8;
    uint B : 8;
    uint A : 8;
};

RWStructuredBuffer<uint> buf : r0;

[numthreads(1,1,1)]
void main()
{
    ColorRGBA c;
    c.R = 127;
    buf[0] = (uint)c;
}

// CHECK: OpReturn

