// Run: %dxc -T ps_6_0 -E main

void main() {
    abort();
}

// CHECK: :4:5: error: no equivalent for abort intrinsic function in Vulkan
