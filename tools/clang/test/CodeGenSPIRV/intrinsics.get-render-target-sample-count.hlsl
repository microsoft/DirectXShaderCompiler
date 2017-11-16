// Run: %dxc -T ps_6_0 -E main

void main() {
    uint a = GetRenderTargetSampleCount();
}

// CHECK: :4:14: error: no equivalent for GetRenderTargetSampleCount intrinsic function in Vulkan
