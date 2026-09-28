// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -E main -fcgl %s | FileCheck %s
// RUN: %dxc -T cs_6_10 -E main %s | FileCheck %s --check-prefix=DXIL

// Explicitly static LinAlg matrix globals are mutable module state, not
// constant buffer data.

#include <dx/linalg.h>
using namespace dx::linalg;

using MatrixTy =
    Matrix<ComponentType::F32, 4, 4, MatrixUse::Accumulator,
           MatrixScope::Wave>;

struct MatrixState {
  MatrixTy Matrix;
};

static MatrixTy GlobalMatrix;
static MatrixTy GlobalMatrixArray[2];
static MatrixState GlobalMatrixState;
RWByteAddressBuffer Output;

[numthreads(1, 1, 1)]
void main() {
  GlobalMatrix = MatrixTy::Splat(1.0f);
  Output.Store(0, GlobalMatrix.Get(0));
}

// CHECK-NOT: dx.hl.subscript.cb
// CHECK: @GlobalMatrix = internal global
// CHECK-NOT: dx.hl.subscript.cb
// CHECK: bitcast {{.*}} @GlobalMatrix
// CHECK: call float {{.*}} @GlobalMatrix
// CHECK-NOT: dx.hl.subscript.cb

// DXIL: call %dx.types.LinAlgMatrixC9M4N4U2S1 @dx.op.linAlgFillMatrix
// DXIL: call float @dx.op.linAlgMatrixGetElement
// DXIL-NOT: @dx.op.cbufferLoad
