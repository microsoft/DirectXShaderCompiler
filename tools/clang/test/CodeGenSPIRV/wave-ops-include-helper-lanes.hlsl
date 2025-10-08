// RUN: %dxc -T ps_6_7 -E main -spirv %s | FileCheck %s

// CHECK-NOT: OpCapability MaximalReconvergenceKHR
// CHECK: OpCapability QuadControlKHR
// CHECK: OpExtension "SPV_KHR_maximal_reconvergence"
// CHECK: OpExtension "SPV_KHR_quad_control"
// CHECK: OpExecutionMode %main MaximallyReconvergesKHR
// CHECK: OpExecutionMode %main RequireFullQuadsKHR

[WaveOpsIncludeHelperLanes]
float4 main(float4 pos : SV_Position) : SV_Target
{
    return pos;
}
