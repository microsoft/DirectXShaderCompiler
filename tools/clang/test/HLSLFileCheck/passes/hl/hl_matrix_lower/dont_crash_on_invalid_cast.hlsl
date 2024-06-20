// RUN: %dxc -T cs_6_0 %s | xfailcat | FileCheck %s -input-file=stderr

// The HL matrix lowering pass can sometimes throw an exception
// due to an invalid LLVM-level cast<Ty> call.  Make sure that
// propagates out to a user-level error.

// Note: There is still a bug in the compiler here.  Not all matrix
// lowerings are covered by the pass.


//   CHECK: error: cast<X>() argument of incompatible type

struct a {
  float b;
  float2x4 c;
};
struct d {
  a c;
};
struct e {
  d c[1];
  d f[1];
};

static e g = (e)0;

[numthreads(1, 1, 1)]
void main() {
  d h = g.f[0];
  return;
}

