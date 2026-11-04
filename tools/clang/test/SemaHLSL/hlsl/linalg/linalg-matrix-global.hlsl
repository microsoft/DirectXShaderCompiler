// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -verify %s
// RUN: %dxc -T lib_6_10 -verify -ast-dump %s | FileCheck %s

// Global declarations of types containing LinAlg matrices must not be
// classified as numeric when determining whether a global is implicitly const.

#include <dx/linalg.h>
using namespace dx::linalg;

// expected-error@+1 {{global variable 'g_mat' containing a linear algebra matrix must be declared 'static'}}
Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread> g_mat;

struct S {
  __builtin_LinAlgMatrix
      [[__LinAlgMatrix_Attributes(ComponentType::F16, 4, 4, MatrixUse::A,
                                  MatrixScope::Thread)]] handle;
};

// expected-error@+1 {{global variable 'g_s' containing a linear algebra matrix must be declared 'static'}}
S g_s;

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread> local_mat;
  S local_s;
}

// Invalid globals are omitted from the AST after their required diagnostics;
// check the corresponding local types here to ensure they remain unqualified.
// CHECK: VarDecl {{.*}} local_mat 'Matrix<ComponentType::F16, 4, 4, MatrixUse::A, MatrixScope::Thread>':
// CHECK: VarDecl {{.*}} local_s 'S'
