// Run: %dxc -T as_6_5 -E main

// CHECK:  11:6: error: AS entry point must have the numthreads attribute

struct MeshPayload {
    float4 pos;
};

groupshared MeshPayload pld;

void main(
        in uint tig : SV_GroupIndex)
{
}
