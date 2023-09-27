// RUN: %dxc -T lib_6_5 %s | FileCheck %s

// CHECK: error: amplification entry point must have the numthreads attribute


struct Payload {
    float2 dummy;
};

//[numthreads(8, 1, 1)]
[shader("amplification")]
void main()
{
    Payload pld;
    pld.dummy = float2(1.0,2.0);
    DispatchMesh(8, 1, 1, pld);
}
