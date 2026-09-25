// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -verify %s

// Global declarations of types containing LinAlg matrices must not be
// classified as numeric when determining whether a global is implicitly const.

// expected-no-diagnostics

#include <dx/linalg.h>
using namespace dx::linalg;

Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread> g_mat;

struct S {
  __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(ComponentType::F16, 4, 4, MatrixUse::A,
                                  MatrixScope::Thread)]] handle;
};

S g_s;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {}
