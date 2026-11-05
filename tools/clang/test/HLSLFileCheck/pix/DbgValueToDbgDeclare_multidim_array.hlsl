// RUN: %dxc -T cs_6_0 -Od -Zi %s | %opt -S -dxil-dbg-value-to-dbg-declare | %FileCheck %s

static float MyArray[2][2] = {
    { 1.0, 2.0 },
    { 3.0, 4.0 }
};

RWByteAddressBuffer RawUAV : register(u0);

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    RawUAV.Store(0, asuint(MyArray[tid.x][tid.y]));
}

// Verify stores for every flattened element of the multidimensional array.
// CHECK: store float 2.000000e+00, float*
// CHECK: store float 3.000000e+00, float*
// CHECK: store float 4.000000e+00, float*
