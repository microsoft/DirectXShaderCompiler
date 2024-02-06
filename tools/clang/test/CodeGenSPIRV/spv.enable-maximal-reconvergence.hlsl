// RUN: %dxc -T cs_6_0 -E main -fspv-enable-maximal-reconvergence %s -spirv | FileCheck %s

// CHECK:       OpExtension "SPV_KHR_maximal_reconvergence"
// CHECK:       OpEntryPoint GLCompute %main "main"
// CHECK:       OpExecutionMode %main MaximallyReconvergesKHR
[numthreads(1, 1, 1)]
void main() {}

