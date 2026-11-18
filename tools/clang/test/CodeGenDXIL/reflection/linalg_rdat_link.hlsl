// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_10 -DLIB_A %s -Fo %t.a
// RUN: %dxc -T lib_6_10 -DLIB_OVERLAP %s -Fo %t.overlap
// RUN: %dxc -T lib_6_10 -DLIB_DISJOINT %s -Fo %t.disjoint
// RUN: %dxl -T lib_6_10 "%t.a;%t.overlap" -Fo %t.overlap-linked
// RUN: %dxa %t.overlap-linked -dumprdat | FileCheck %s --check-prefix=OVERLAP
// RUN: %dxl -T lib_6_10 "%t.a;%t.disjoint" -Fo %t.disjoint-linked
// RUN: %dxa %t.disjoint-linked -dumprdat | FileCheck %s --check-prefix=DISJOINT

#include <dx/linalg.h>
using namespace dx::linalg;

#if defined(LIB_A) || defined(LIB_OVERLAP)

using WaveA =
    Matrix<ComponentType::F32, 2, 4, MatrixUse::A, MatrixScope::Wave>;
using WaveB =
    Matrix<ComponentType::I32, 4, 3, MatrixUse::B, MatrixScope::Wave>;
using WaveAccumulator =
    Matrix<ComponentType::F32, 2, 3, MatrixUse::Accumulator, MatrixScope::Wave>;

#endif

#ifdef LIB_A

RWByteAddressBuffer OutputA : register(u0);

[shader("compute")]
[numthreads(1, 1, 1)]
void EntryA() {
  WaveA A = WaveA::Splat(1.0f);
  WaveB B = WaveB::Splat(2);
  WaveAccumulator C = Multiply<ComponentType::F32>(A, B);
  C.InterlockedAccumulate(OutputA, 0, 12, MatrixLayout::RowMajor);
}

#endif

#ifdef LIB_OVERLAP

RWByteAddressBuffer OutputOverlap : register(u1);

[shader("compute")]
[numthreads(1, 1, 1)]
void EntryOverlap() {
  WaveA A = WaveA::Splat(3.0f);
  WaveB B = WaveB::Splat(4);
  WaveAccumulator C = Multiply<ComponentType::F32>(A, B);
  C.InterlockedAccumulate(OutputOverlap, 0, 12, MatrixLayout::RowMajor);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void NoLinAlgOverlap() {
}

#endif

#ifdef LIB_DISJOINT

RWByteAddressBuffer OutputDisjoint : register(u2);

using GroupA =
    Matrix<ComponentType::I32, 5, 7, MatrixUse::A, MatrixScope::ThreadGroup>;
using GroupB =
    Matrix<ComponentType::U32, 7, 6, MatrixUse::B, MatrixScope::ThreadGroup>;
using GroupAccumulator =
    Matrix<ComponentType::I32, 5, 6, MatrixUse::Accumulator,
           MatrixScope::ThreadGroup>;

[shader("compute")]
[numthreads(1, 1, 1)]
void EntryDisjoint() {
  GroupA A = GroupA::Splat(5);
  GroupB B = GroupB::Splat(6u);
  GroupAccumulator C = Multiply<ComponentType::I32>(A, B);
  C.InterlockedAccumulate(OutputDisjoint, 0, 24, MatrixLayout::RowMajor);
}

[shader("compute")]
[numthreads(1, 1, 1)]
void NoLinAlgDisjoint() {
}

#endif

// Identical runtime data from two input libraries aliases one global record
// set while remaining referenced by both shader functions.
// OVERLAP: RecordTable (stride = 56 bytes) FunctionTable[3] = {
// OVERLAP-LABEL: UnmangledName: "EntryA"
// OVERLAP: ExtendedProperties: <[[OVERLAP_PROPS:[0-9]+]]:RecordArrayRef<ExtendedFunctionProperties>[3]>
// OVERLAP: PropertyType: LinAlgMatrixConstruction
// OVERLAP: PropertyType: LinAlgWaveMatrixMultiply
// OVERLAP: PropertyType: LinAlgAccumulateStore
// OVERLAP-LABEL: UnmangledName: "EntryOverlap"
// OVERLAP: ExtendedProperties: <[[OVERLAP_PROPS]]:RecordArrayRef<ExtendedFunctionProperties>[3]>
// OVERLAP-LABEL: UnmangledName: "NoLinAlgOverlap"
// OVERLAP: ExtendedProperties: <RecordArrayRef<ExtendedFunctionProperties>[0]> = {}
// OVERLAP-DAG: RecordTable (stride = 8 bytes) ExtendedFunctionPropertiesTable[3] = {
// OVERLAP-DAG: RecordTable (stride = 12 bytes) LinAlgMatrixOperationShapeTable[3] = {
// OVERLAP-DAG: RecordTable (stride = 8 bytes) LinAlgMatrixConstructionTable[2] = {
// OVERLAP-DAG: RecordTable (stride = 8 bytes) LinAlgWaveMatrixMultiplyTable[1] = {
// OVERLAP-DAG: RecordTable (stride = 4 bytes) LinAlgAccumulateStoreTable[1] = {
// OVERLAP-NOT: LinAlgThreadGroupMatrixMultiplyTable

// Disjoint input libraries retain both independent record sets. The negative
// entry remains empty after linking and RDAT regeneration.
// DISJOINT: RecordTable (stride = 56 bytes) FunctionTable[3] = {
// DISJOINT-LABEL: UnmangledName: "EntryA"
// DISJOINT: ExtendedProperties: {{.*}}[3]
// DISJOINT: PropertyType: LinAlgMatrixConstruction
// DISJOINT: PropertyType: LinAlgWaveMatrixMultiply
// DISJOINT: PropertyType: LinAlgAccumulateStore
// DISJOINT-LABEL: UnmangledName: "EntryDisjoint"
// DISJOINT: ExtendedProperties: {{.*}}[3]
// DISJOINT: PropertyType: LinAlgMatrixConstruction
// DISJOINT: PropertyType: LinAlgThreadGroupMatrixMultiply
// DISJOINT: PropertyType: LinAlgAccumulateStore
// DISJOINT-LABEL: UnmangledName: "NoLinAlgDisjoint"
// DISJOINT: ExtendedProperties: <RecordArrayRef<ExtendedFunctionProperties>[0]> = {}
// DISJOINT-DAG: RecordTable (stride = 8 bytes) ExtendedFunctionPropertiesTable[6] = {
// DISJOINT-DAG: RecordTable (stride = 12 bytes) LinAlgMatrixOperationShapeTable[6] = {
// DISJOINT-DAG: RecordTable (stride = 8 bytes) LinAlgMatrixConstructionTable[4] = {
// DISJOINT-DAG: RecordTable (stride = 8 bytes) LinAlgWaveMatrixMultiplyTable[1] = {
// DISJOINT-DAG: RecordTable (stride = 8 bytes) LinAlgThreadGroupMatrixMultiplyTable[1] = {
// DISJOINT-DAG: RecordTable (stride = 4 bytes) LinAlgAccumulateStoreTable[2] = {
