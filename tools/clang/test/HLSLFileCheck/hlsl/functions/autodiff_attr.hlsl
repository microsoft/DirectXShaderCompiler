// RUN: %dxc -T ps_6_0 -E main %s 2>&1 | FileCheck %s

// Sanity check: the [[dxc::AutoDiff(...)]] attribute is parsed and accepted
// in normal compilation, but otherwise has no effect on codegen. The shader
// should compile cleanly and produce DXIL.

// CHECK: target triple

[[dxc::AutoDiff(fwd)]]
float forward_only(float x) { return x * x; }

[[dxc::AutoDiff(bwd)]]
float backward_only(float x) { return x + x; }

[[dxc::AutoDiff(fwd, bwd)]]
float both(float x) { return x * 2.0; }

[[dxc::AutoDiff(bwd, fwd)]]
float reversed(float x) { return x - 1.0; }

float main(float x : A) : SV_Target {
  return forward_only(x) + backward_only(x) + both(x) + reversed(x);
}
