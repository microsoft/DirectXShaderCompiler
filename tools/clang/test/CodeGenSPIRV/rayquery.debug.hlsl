// RUN: %dxc -T cs_6_5 -E main -fspv-target-env=vulkan1.2 -fspv-debug=vulkan-with-source -spirv %s | FileCheck %s

// CHECK: [[_:%[0-9]+]] = OpString "rayQueryKHR"

[numthreads(64, 1, 1)]
void main() {
    RayQuery<RAY_FLAG_NONE> q;
}
