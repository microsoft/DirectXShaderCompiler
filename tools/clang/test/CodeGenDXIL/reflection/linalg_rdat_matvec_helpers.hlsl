// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -Vd %s -Fo %t.lib
// RUN: %dxa %t.lib -dumprdat | FileCheck %s --check-prefix=RDAT

#include <dx/linalg.h>
using namespace dx::linalg;

ByteAddressBuffer Input;
RWStructuredBuffer<float4> Output;

using ThreadA =
    Matrix<ComponentType::F32, 4, 4, MatrixUse::A, MatrixScope::Thread>;

[noinline] void Leaf(uint Index) {
  ThreadA A = ThreadA::Load<MatrixLayout::MulOptimal>(Input, 0, 0);
  Output[Index] = Multiply<float>(A, (float4)1.0f);
}

[noinline] void Middle(uint Index) {
  Leaf(Index);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void main(uint Index : SV_GroupIndex) {
  Middle(Index);
}

// Inserting a matvec requirement with zero flags must still propagate a change
// through every caller, and all three functions share the same property record.
// RDAT: FunctionTable[3]
// RDAT-LABEL: UnmangledName: "Leaf"
// RDAT: ExtendedProperties: <[[PROPS:[0-9]+]]:RecordArrayRef<ExtendedFunctionProperties>[1]>
// RDAT: PropertyType: LinAlgThreadMatrixVectorMultiply
// RDAT: ResultType: F32
// RDAT: MatrixType: F32
// RDAT: VectorInputType: F32
// RDAT: Flags: 0 (None)
// RDAT-LABEL: UnmangledName: "Middle"
// RDAT: ExtendedProperties: <[[PROPS]]:RecordArrayRef<ExtendedFunctionProperties>[1]>
// RDAT-LABEL: UnmangledName: "main"
// RDAT: ExtendedProperties: <[[PROPS]]:RecordArrayRef<ExtendedFunctionProperties>[1]>
// RDAT: LinAlgThreadMatrixVectorMultiplyTable[1]
