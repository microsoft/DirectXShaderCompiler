// Run: %dxc -T lib_6_4 -E main

[shader("compute")]
[numthreads(16, 16, 1)]
void entryHistogram(uint3 id: SV_DispatchThreadID, uint idx: SV_GroupIndex)
{
}

// CHECK: 11:6: error: thread group size [numthreads(x,y,z)] is missing from the entry-point function
[shader("compute")]
void entryAverage(uint3 id: SV_DispatchThreadID, uint idx: SV_GroupIndex)
{
}

