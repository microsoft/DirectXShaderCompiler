// Run: %dxc -T cs_6_0 -E main

// According to the SPIR-V spec (2.14. Relaxed Precision):
// The RelaxedPrecision decoration can be applied to:
// The Result <id> of an instruction that operates on numerical types, meaning
// the instruction is to operate at relaxed precision. The instruction's
// operands may also be truncated to the relaxed precision.
//
// In this example, (a > 0.0) comparison is operating on floats with relaxed
// precision, and should therefore have the decoration. However, the "any"
// intrinsic function is operating on a vector of booleans, and it should not
// be decorated.

// CHECK:     OpDecorate %a RelaxedPrecision
// CHECK:     OpDecorate [[a:%\d+]] RelaxedPrecision
// CHECK:     OpDecorate [[compare_op:%\d+]] RelaxedPrecision
//
// We should NOT have a decoration for the 'any' operation.
//
// CHECK-NOT: OpDecorate {{%\d+}} RelaxedPrecision

// CHECK:           [[a]] = OpLoad %v2float %a
// CHECK:  [[compare_op]] = OpFOrdGreaterThan %v2bool [[a]] {{%\d+}}
// CHECK: [[any_op:%\d+]] = OpAny %bool [[compare_op]]

RWBuffer<float2> Buf;

[numthreads(1, 1, 1)]
void main() {
  min16float2 a = Buf[0];
  if (any(a > 0.0))
    Buf[0] = 1;
}
