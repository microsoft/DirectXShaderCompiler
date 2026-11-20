// RUN: %dxr %s > %t.base
// RUN: %dxr -generate-differentials %s > %t.gen
// RUN: diff %t.base %t.gen
// RUN: %dxr -generate-differentials %s | FileCheck %s
// RUN: %dxr -generate-differentials -generated-only %s | count 0

// When the translation unit contains no functions annotated with
// [[dxc::AutoDiff(...)]], the -generate-differentials pass must not modify
// the rewriter output at all: it produces the same bytes as a plain `dxr`
// invocation, with no auto-diff header includes and no `user::ad::{fwd,bwd}`
// namespace blocks appended.
//
// The `diff` RUN line above is the strongest form of the assertion (byte
// equality against the unmodified rewriter output). The FileCheck RUN line
// below adds a redundant guard against future regressions where the
// generator might silently emit auto-diff scaffolding.

// CHECK-NOT: #include <ad/fwd>
// CHECK-NOT: #include <ad/bwd>
// CHECK-NOT: namespace user
// CHECK-NOT: Value<
// CHECK-NOT: Variable<
// CHECK-NOT: GradientContext

// CHECK: float helper(float x)
// CHECK: return x * x;
// CHECK: float main(float x : A) : SV_Target
// CHECK: return helper(x);

float helper(float x) { return x * x; }

float main(float x : A) : SV_Target { return helper(x); }
