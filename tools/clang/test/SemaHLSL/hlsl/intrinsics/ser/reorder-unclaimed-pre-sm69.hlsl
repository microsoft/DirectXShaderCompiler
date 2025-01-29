// RUN: %dxc -T lib_6_6 %s -verify
// RUN: %dxc -T lib_6_8 %s -verify

// Check that intrinsic names of Shader Execution Reordering are unclaimed pre SM 6.9.
// expected-no-diagnostics

void ReorderThread(uint CoherenceHint, uint NumCoherenceHintBitsFromLSB) {
}

[shader("raygeneration")]
void main() {
  ReorderThread(15u, 4u);
}
