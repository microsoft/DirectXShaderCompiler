// RUN: %dxc -T cs_6_0 -E main %s -spirv | FileCheck %s

// CHECK-NOT:       OpExtension "SPV_KHR_maximal_reconvergence"
// CHECK:           OpEntryPoint GLCompute %main "main"
// CHECK-NOT:       OpExecutionMode %main MaximallyReconvergesKHR
[numthreads(1, 1, 1)]
void main() {}


