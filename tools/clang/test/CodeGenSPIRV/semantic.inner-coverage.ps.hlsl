// Run: %dxc -T ps_6_0 -E main

float4 main(uint inCov : SV_InnerCoverage) : SV_Target {
    return 1.0;
}

// CHECK: 3:26: error: no equivalent for semantic SV_InnerCoverage in Vulkan
