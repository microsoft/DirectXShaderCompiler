// RUN: %dxc -T ps_6_0 -E main

// This test checks if capability visitor emits SparseResidency capability
// correctly when OpImageSparseTexelsResident is used.

// CHECK: OpCapability SparseResidency

void main() {
  uint status;

// CHECK: [[residency_code:%\d+]] = OpLoad %uint %status
// CHECK:         [[result:%\d+]] = OpImageSparseTexelsResident %bool [[residency_code]]
// CHECK:                           OpStore %result [[result]]
  bool result = CheckAccessFullyMapped(status);
}
