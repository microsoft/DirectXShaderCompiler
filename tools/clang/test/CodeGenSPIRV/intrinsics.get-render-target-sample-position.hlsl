// Run: %dxc -T ps_6_0 -E main

void main() {
    uint a = GetRenderTargetSamplePosition(2);
}

// CHECK: :4:14: error: no equivalent for GetRenderTargetSamplePosition intrinsic function in Vulkan
