// Run: %dxc -T ms_6_5 -E main

// CHECK: 10:6: error: expected vertices object declaration

#define MAX_PRIM 81
#define NUM_THREADS 128

[outputtopology("triangle")]
[numthreads(NUM_THREADS, 1, 1)]
void main(
        out indices uint3 primitiveInd[MAX_PRIM])
{
}
