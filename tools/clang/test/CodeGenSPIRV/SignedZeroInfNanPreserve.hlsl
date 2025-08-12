// RUN: %dxc -T cs_6_0 -spirv -Gis %s| FileCheck %s --check-prefixes=CHECK,OLD
// RUN: %dxc -T cs_6_0 -spirv -Gis -fspv-target-env=vulkan1.2 %s| FileCheck %s --check-prefixes=CHECK,NEW

// OLD-NOT: OpCapability SignedZeroInfNanPreserve
// NEW: OpCapability SignedZeroInfNanPreserve
// CHECK: OpEntryPoint
// OLD-NOT: OpExecutionMode %main SignedZeroInfNanPreserve 32
// NEW: OpExecutionMode %main SignedZeroInfNanPreserve 32

[numthreads(8,1,1)]
void main() {}
