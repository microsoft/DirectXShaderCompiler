// Run: %dxc -T ps_6_0 -E main

// CHECK: OpCapability SampleMaskPostDepthCoverage
// CHECK: OpExtension "SPV_KHR_post_depth_coverage"
// CHECK: OpExecutionMode %main PostDepthCoverage

[[vk::post_depth_coverage]]
float4 main() : SV_Target { return 1.0; }
