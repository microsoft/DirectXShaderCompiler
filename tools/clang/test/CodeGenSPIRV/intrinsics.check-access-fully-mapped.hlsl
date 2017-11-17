// Run: %dxc -T ps_6_0 -E main

void main() {
  uint status;
  bool fullyMapped = CheckAccessFullyMapped(status);
}

// CHECK: error: no equivalent for CheckAccessFullyMapped intrinsic function in Vulkan