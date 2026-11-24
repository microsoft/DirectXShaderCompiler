// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -enable-16bit-types -Vd %s -Fo %t
// RUN: %dxa %t -dumprdat | FileCheck %s

#include <dx/linalg.h>
using namespace dx::linalg;

ByteAddressBuffer Input;
RWByteAddressBuffer Output;
RWStructuredBuffer<vector<half, 8> > VectorOutput;
groupshared float SharedFloat[64];

using ThreadA =
    Matrix<ComponentType::F16, 8, 4, MatrixUse::A, MatrixScope::Thread>;
using ThreadAccumulator = Matrix<ComponentType::F32, 4, 4,
                                 MatrixUse::Accumulator, MatrixScope::Thread>;

using WaveA =
    Matrix<ComponentType::F16, 3, 4, MatrixUse::A, MatrixScope::Wave>;
using WaveB =
    Matrix<ComponentType::I32, 4, 5, MatrixUse::B, MatrixScope::Wave>;
using WaveAccumulator =
    Matrix<ComponentType::F32, 3, 5, MatrixUse::Accumulator, MatrixScope::Wave>;
using WaveA2 =
    Matrix<ComponentType::F16, 2, 4, MatrixUse::A, MatrixScope::Wave>;
using WaveB2 =
    Matrix<ComponentType::I32, 4, 6, MatrixUse::B, MatrixScope::Wave>;
using WaveAccumulator2 =
    Matrix<ComponentType::F32, 2, 6, MatrixUse::Accumulator, MatrixScope::Wave>;

using GroupA =
    Matrix<ComponentType::I32, 2, 4, MatrixUse::A, MatrixScope::ThreadGroup>;
using GroupB =
    Matrix<ComponentType::U32, 4, 3, MatrixUse::B, MatrixScope::ThreadGroup>;
using GroupAccumulator =
    Matrix<ComponentType::I32, 2, 3, MatrixUse::Accumulator,
           MatrixScope::ThreadGroup>;

[noinline] void ThreadOps(uint Index) {
  ThreadA A =
      ThreadA::Load<MatrixLayout::MulOptimalTranspose>(Input, 0, 0);
  VectorOutput[Index] = Multiply<half>(A, (vector<half, 4>)1.0h);
  ThreadA RowMajor = ThreadA::Load<MatrixLayout::RowMajor>(Input, 64, 16);
  VectorOutput[Index] = Multiply<half>(RowMajor, (vector<half, 4>)2.0h);

  ThreadAccumulator Outer =
      OuterProduct<ComponentType::F32>((float4)2.0f, (float4)3.0f);
  Outer.InterlockedAccumulate(Output, 128);
  InterlockedAccumulate(Output, 256, (int4)Index);
}

[noinline] void WaveOps() {
  WaveA A = WaveA::Splat(1.0h);
  WaveB B = WaveB::Splat(2);
  WaveAccumulator C = Multiply<ComponentType::F32>(A, B);
  C.InterlockedAccumulate(Output, 0, 20, MatrixLayout::RowMajor);
  C.InterlockedAccumulate(SharedFloat, 0, 20, MatrixLayout::RowMajor);

  WaveA2 A2 = WaveA2::Splat(3.0h);
  WaveB2 B2 = WaveB2::Splat(4);
  WaveAccumulator2 C2 = Multiply<ComponentType::F32>(A2, B2);
  C2.InterlockedAccumulate(Output, 64, 24, MatrixLayout::RowMajor);
}

[noinline] void GroupOps() {
  GroupA A = GroupA::Splat(4);
  GroupB B = GroupB::Splat(5u);
  GroupAccumulator C = Multiply<ComponentType::I32>(A, B);
  C.Store(Output, 512, 12, MatrixLayout::RowMajor);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void AllOps(uint Index : SV_GroupIndex) {
  ThreadOps(Index);
  WaveOps();
  GroupOps();
}

[shader("compute")]
[numthreads(1, 1, 1)]
void WaveOnly() {
  WaveOps();
}

[shader("compute")]
[numthreads(1, 1, 1)]
void NoLinAlg() {
}

// CHECK: RecordTable (stride = 56 bytes) FunctionTable[6] = {

// Direct thread-scope usage is recorded on the helper.
// CHECK-LABEL: UnmangledName: "ThreadOps"
// CHECK: ExtendedProperties: {{.*}}[3]
// CHECK: PropertyType: LinAlgThreadMatrixVectorMultiply
// CHECK: ResultType: F16
// CHECK: MatrixType: F16
// CHECK: VectorInputType: F16
// CHECK: Flags: (MatrixTransposed | MatrixNonMulOptimalLayout)
// CHECK: PropertyType: LinAlgOuterProduct
// CHECK: ResultType: F32
// CHECK: VectorInputType: F32
// CHECK: PropertyType: LinAlgAccumulateStore
// CHECK: AccumulatorType: I32
// CHECK: Flags: (RawBuffer)
// CHECK: AccumulatorType: F32
// CHECK: Flags: (RawBuffer)

// Wave construction, multiply, and both store destinations are merged.
// CHECK-LABEL: UnmangledName: "WaveOps"
// CHECK: ExtendedProperties: <[[WAVE_PROPS:[0-9]+]]:RecordArrayRef<ExtendedFunctionProperties>[3]>
// CHECK: PropertyType: LinAlgMatrixConstruction
// CHECK: M: 0
// CHECK-NEXT: N: 5
// CHECK-NEXT: K: 4
// CHECK: M: 0
// CHECK-NEXT: N: 6
// CHECK-NEXT: K: 4
// CHECK: MatrixType: I32
// CHECK: M: 2
// CHECK-NEXT: N: 0
// CHECK-NEXT: K: 4
// CHECK: M: 3
// CHECK-NEXT: N: 0
// CHECK-NEXT: K: 4
// CHECK: MatrixType: F16
// CHECK: PropertyType: LinAlgWaveMatrixMultiply
// CHECK: M: 2
// CHECK-NEXT: N: 6
// CHECK-NEXT: K: 4
// CHECK: M: 3
// CHECK-NEXT: N: 5
// CHECK-NEXT: K: 4
// CHECK: AccumulatorType: F32
// CHECK: MatrixAType: F16
// CHECK: MatrixBType: I32
// CHECK: PropertyType: LinAlgAccumulateStore
// CHECK: AccumulatorType: F32
// CHECK: Flags: (RawBuffer | GroupShared)

// Threadgroup construction keeps A, B, and accumulator uses distinct.
// CHECK-LABEL: UnmangledName: "GroupOps"
// CHECK: ExtendedProperties: {{.*}}[2]
// CHECK: PropertyType: LinAlgMatrixConstruction
// CHECK: M: 2
// CHECK-NEXT: N: 0
// CHECK-NEXT: K: 4
// CHECK: MatrixType: I32
// CHECK: M: 2
// CHECK-NEXT: N: 3
// CHECK-NEXT: K: 0
// CHECK: MatrixType: I32
// CHECK: M: 0
// CHECK-NEXT: N: 3
// CHECK-NEXT: K: 4
// CHECK: MatrixType: U32
// CHECK: PropertyType: LinAlgThreadGroupMatrixMultiply
// CHECK: AccumulatorType: I32
// CHECK: MatrixAType: I32
// CHECK: MatrixBType: U32

// The entry point receives the union of all transitive helper usage.
// CHECK-LABEL: UnmangledName: "AllOps"
// CHECK: ExtendedProperties: {{.*}}[6]
// CHECK: PropertyType: LinAlgMatrixConstruction
// CHECK: PropertyType: LinAlgThreadMatrixVectorMultiply
// CHECK: PropertyType: LinAlgWaveMatrixMultiply
// CHECK: PropertyType: LinAlgThreadGroupMatrixMultiply
// CHECK: PropertyType: LinAlgOuterProduct
// CHECK: PropertyType: LinAlgAccumulateStore
// CHECK: AccumulatorType: I32
// CHECK: Flags: (RawBuffer)
// CHECK: AccumulatorType: F32
// CHECK: Flags: (RawBuffer | GroupShared)

// The narrower entry point receives only WaveOps data and reuses its records.
// CHECK-LABEL: UnmangledName: "WaveOnly"
// CHECK: ExtendedProperties: <[[WAVE_PROPS]]:RecordArrayRef<ExtendedFunctionProperties>[3]>
// CHECK: PropertyType: LinAlgMatrixConstruction
// CHECK: PropertyType: LinAlgWaveMatrixMultiply
// CHECK: PropertyType: LinAlgAccumulateStore
// CHECK-NOT: PropertyType: LinAlgThreadMatrixVectorMultiply
// CHECK-NOT: PropertyType: LinAlgThreadGroupMatrixMultiply
// CHECK-NOT: PropertyType: LinAlgOuterProduct

// A function without LinAlg usage must not inherit unrelated module data.
// CHECK-LABEL: UnmangledName: "NoLinAlg"
// CHECK: FeatureInfo2: 0
// CHECK: ExtendedProperties: <RecordArrayRef<ExtendedFunctionProperties>[0]> = {}

// Global records are deduplicated across functions and use aligned strides.
// CHECK-DAG: RecordTable (stride = 8 bytes) ExtendedFunctionPropertiesTable[10] = {
// CHECK-DAG: RecordTable (stride = 12 bytes) LinAlgMatrixOperationShapeTable[9] = {
// CHECK-DAG: RecordTable (stride = 8 bytes) LinAlgMatrixConstructionTable[5] = {
// CHECK-DAG: RecordTable (stride = 4 bytes) LinAlgThreadMatrixVectorMultiplyTable[1] = {
// CHECK-DAG: RecordTable (stride = 8 bytes) LinAlgWaveMatrixMultiplyTable[1] = {
// CHECK-DAG: RecordTable (stride = 8 bytes) LinAlgThreadGroupMatrixMultiplyTable[1] = {
// CHECK-DAG: RecordTable (stride = 4 bytes) LinAlgOuterProductTable[1] = {
// CHECK-DAG: RecordTable (stride = 4 bytes) LinAlgAccumulateStoreTable[3] = {
