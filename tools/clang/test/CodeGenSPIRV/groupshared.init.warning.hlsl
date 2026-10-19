// RUN: %dxc -T cs_6_0 -E main -spirv -verify %s

groupshared uint testing = 0;
// expected-warning@-1 {{initializer of 'groupshared' variable will be ignored}}

static groupshared uint staticTesting = 1;
// expected-warning@-1 {{initializer of 'groupshared' variable will be ignored}}

[numthreads(64, 1, 1)]
void main(uint local_thread_id_flat : SV_GroupIndex) {
    
    InterlockedAdd(testing, staticTesting);
    GroupMemoryBarrierWithGroupSync();
    
    if (local_thread_id_flat == 0) {
        if (testing > 64) {
            printf("testing is %u wtf", testing);
        }
    }
}
