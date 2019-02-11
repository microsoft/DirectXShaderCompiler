// Run: %dxc -T cs_6_0 -E main -Zi

#include "DoesntExist.hlsl"

void main() {}


// CHECK: 3:10: fatal error: 'DoesntExist.hlsl' file not found
