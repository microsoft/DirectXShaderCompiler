// RUN: %dxc -T ps_6_0 -E main

void main() {
  uint64_t   uint_1;
  int fbl = firstbitlow(uint_1);
}

// CHECK: error: firstbitlow is not yet implemented for 64-bit width components when targetting SPIR-V