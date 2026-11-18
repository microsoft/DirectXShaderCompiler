// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -Vd %s -Fo %t
// RUN: %dxa %t -dumprdat | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

ByteAddressBuffer Input;
RWStructuredBuffer<float4> Output;

using ThreadA =
    Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Thread>;

[noinline] void ConsumeMatrix(ThreadA A) {
  Output[0] = Multiply<float>(A, (float4)1.0f);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  ThreadA A =
      ThreadA::Load<MatrixLayout::MulOptimalTranspose>(Input, 0, 0);
  ConsumeMatrix(A);
}

// A matrix argument has no local load producer in ConsumeMatrix. Both layout
// flags must be reported conservatively instead of claiming MulOptimal-only.
// CHECK: RecordTable (stride = 56 bytes) FunctionTable[2] = {
// CHECK-LABEL: UnmangledName: "ConsumeMatrix"
// CHECK: ExtendedProperties: <[[PROPS:[0-9]+]]:RecordArrayRef<ExtendedFunctionProperties>[1]>
// CHECK: PropertyType: LinAlgThreadMatrixVectorMultiply
// CHECK: Flags: (MatrixTransposed | MatrixNonMulOptimalLayout)
// CHECK-LABEL: UnmangledName: "main"
// CHECK: ExtendedProperties: <[[PROPS]]:RecordArrayRef<ExtendedFunctionProperties>[1]>
// CHECK: RecordTable (stride = 4 bytes) LinAlgThreadMatrixVectorMultiplyTable[1] = {
// CHECK: Flags: (MatrixTransposed | MatrixNonMulOptimalLayout)
