// REQUIRES: dxil-1-10
// RUN: %dxc -T lib_6_9 -validator-version 1.9 %s -Fo %t.old
// RUN: %dxa %t.old -dumprdat | FileCheck %s --check-prefix=OLD
// RUN: %dxc -T lib_6_10 -validator-version 1.10 %s -Fo %t.empty
// RUN: %dxa %t.empty -dumprdat | FileCheck %s --check-prefix=EMPTY
// RUN: %dxc -T lib_6_10 -validator-version 1.10 -DLINALG %s -Fo %t.linalg
// RUN: %dxa %t.linalg -dumprdat | FileCheck %s --check-prefix=LINALG

#ifdef LINALG

#include <dx/linalg.h>
using namespace dx::linalg;

RWByteAddressBuffer Output;

using WaveA =
    Matrix<ComponentType::F32, 2, 4, MatrixUse::A, MatrixScope::Wave>;
using WaveB =
    Matrix<ComponentType::I32, 4, 3, MatrixUse::B, MatrixScope::Wave>;
using WaveAccumulator =
    Matrix<ComponentType::F32, 2, 3, MatrixUse::Accumulator, MatrixScope::Wave>;

#endif

[shader("compute")]
[numthreads(1, 1, 1)]
void main() {
#ifdef LINALG
  WaveA A = WaveA::Splat(1.0f);
  WaveB B = WaveB::Splat(2);
  WaveAccumulator C = Multiply<ComponentType::F32>(A, B);
  C.InterlockedAccumulate(Output, 0, 12, MatrixLayout::RowMajor);
#endif
}

// Validator versions before 1.10 retain the previous function record and do
// not recognize any of the new parts.
// OLD: <0:RuntimeDataFunctionInfo2> = {
// OLD-NOT: RuntimeDataFunctionInfo3
// OLD-NOT: ExtendedProperties
// OLD-NOT: LinAlgMatrixOperationShapeTable
// OLD-NOT: ExtendedFunctionPropertiesTable

// Validator 1.10 uses FunctionInfo3 even when the property list is empty, and
// empty LinAlg tables are omitted from the serialized container.
// EMPTY: <0:RuntimeDataFunctionInfo3> = {
// EMPTY: ExtendedProperties: <RecordArrayRef<ExtendedFunctionProperties>[0]> = {}
// EMPTY-NOT: LinAlgMatrixOperationShapeTable
// EMPTY-NOT: ExtendedFunctionPropertiesTable

// LinAlg usage activates the new record tables and function property list.
// LINALG: <0:RuntimeDataFunctionInfo3> = {
// LINALG: ExtendedProperties: {{.*}}[3]
// LINALG: PropertyType: LinAlgMatrixConstruction
// LINALG: PropertyType: LinAlgWaveMatrixMultiply
// LINALG: PropertyType: LinAlgAccumulateStore
// LINALG-DAG: RecordTable (stride = 8 bytes) ExtendedFunctionPropertiesTable[3] = {
// LINALG-DAG: RecordTable (stride = 12 bytes) LinAlgMatrixOperationShapeTable[3] = {
// LINALG-DAG: RecordTable (stride = 8 bytes) LinAlgMatrixConstructionTable[2] = {
// LINALG-DAG: RecordTable (stride = 8 bytes) LinAlgWaveMatrixMultiplyTable[1] = {
// LINALG-DAG: RecordTable (stride = 4 bytes) LinAlgAccumulateStoreTable[1] = {
