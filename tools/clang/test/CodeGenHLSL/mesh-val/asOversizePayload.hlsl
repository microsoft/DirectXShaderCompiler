// RUN: %dxc -E main -T as_6_5 %s | FileCheck %s

// CHECK: payload size is greater than 16384

#define NUM_THREADS 32

struct Payload {
    float2 dummy;
    float4 pos[1024];
    float color[2];
};

[numthreads(NUM_THREADS, 1, 1)]
void main()
{
    Payload pld;
    pld.dummy = float2(1.0,2.0);
    pld.pos[0] = float4(3.0,4.0,5.0,6.0);
    pld.color[0] = 7.0;
    pld.color[1] = 8.0;
    DispatchMesh(NUM_THREADS, 1, 1, pld);
}
