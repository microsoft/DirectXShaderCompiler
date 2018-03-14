// Run: %dxc -T vs_6_0 -E main

[[vk::binding(10, 2)]] float4 gVec;

float4 main() : A { return gVec; }

// CHECK: :3:3: error: variable 'gVec' will be placed in $Globals so cannot have vk::binding attribute
