// RUN: %dxc -T lib_6_6 %s -verify
// RUN: %dxc -T lib_6_8 %s -verify

// Check that intrinsic names of Shader Execution Reordering are unclaimed pre SM 6.9.
// expected-no-diagnostics

namespace dx {
void MaybeReorderThread(uint CoherenceHint, uint NumCoherenceHintBitsFromLSB) {
}
}

[shader("raygeneration")]
void main() {
  dx::MaybeReorderThread(15u, 4u);
}
