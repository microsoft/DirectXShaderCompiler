// RUN: %dxc -T cs_6_6 -E main -fspv-extension=SPV_NV_compute_shader_derivatives -fcgl %s -spirv 2>&1 | FileCheck -check-prefix=CHECK-LINEAR %s
// RUN: %dxc -T cs_6_6 -E main -fspv-extension=SPV_NV_compute_shader_derivatives -fcgl -DQUADS %s -spirv 2>&1 | FileCheck -check-prefix=CHECK-QUADS %s

// Note: validation disabled until NodePayloadAMDX pointers are allowed
// as function arguments

// CHECK-LINEAR: OpCapability ComputeDerivativeGroupLinearKHR
// CHECK-LINEAR: OpExecutionMode %{{[^ ]*}} DerivativeGroupLinearKHR
// CHECK-QUADS: OpCapability ComputeDerivativeGroupQuadsKHR
// CHECK-QUADS: OpExecutionMode %{{[^ ]*}} DerivativeGroupQuadsKHR

SamplerState ss : register(s2);
SamplerComparisonState scs;

RWStructuredBuffer<uint> o;
Texture1D        <float>  t1;

#ifdef QUADS
[[vk::constant_id(0)]]
const uint NumThreadsX = 2;
[[vk::constant_id(1)]]
const uint NumThreadsY = 2;
#else
[[vk::constant_id(0)]]
const uint NumThreadsX = 24;
[[vk::constant_id(1)]]
const uint NumThreadsY = 1;
#endif

[numthreads(NumThreadsX,NumThreadsY,1)]
void main(uint3 id : SV_GroupThreadID)
{
    o[0] = t1.Sample(ss, 1);
}

