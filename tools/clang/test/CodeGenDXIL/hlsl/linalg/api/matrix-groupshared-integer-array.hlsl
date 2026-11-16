// REQUIRES: dxil-1-10
// RUN: %dxc -T cs_6_10 -enable-16bit-types -D STORAGE=uint16_t -D COUNT=32 %s | FileCheck %s -DTYPE=i16 -DOVERLOAD=i16
// RUN: %dxc -T cs_6_10 -D STORAGE=uint64_t -D COUNT=8 %s | FileCheck %s -DTYPE=i64 -DOVERLOAD=i64
// RUN: %dxc -T cs_6_10 -D STORAGE=int8_t4_packed -D COUNT=16 %s | FileCheck %s -DTYPE=i32 -DOVERLOAD=i32
// RUN: %dxc -T cs_6_10 -enable-16bit-types -D STORAGE=uint16_t2 -D COUNT=16 %s | FileCheck %s -DTYPE="<2 x i16>" -DOVERLOAD=v2i16
// RUN: %dxc -T cs_6_10 -enable-16bit-types -D STORAGE=uint16_t -D COUNT=192 -D COMPONENT=ComponentType::F32 -D OFFSET=4 -D STRIDE=12 %s | FileCheck %s --check-prefix=FLOAT
// RUN: not %dxc -T cs_6_10 -enable-16bit-types -D STORAGE=uint16_t -D COUNT=16 %s 2>&1 | FileCheck %s --check-prefix=SMALL -DBYTES=32 -DREQUIRED=64

// SMALL: error: Groupshared memory holds '[[BYTES]]' bytes but must hold at least '[[REQUIRED]]' bytes.
// SMALL: error: Groupshared memory holds '[[BYTES]]' bytes but must hold at least '[[REQUIRED]]' bytes.

#include <dx/linalg.h>
using namespace dx::linalg;

#ifndef COMPONENT
#define COMPONENT ComponentType::U8
#endif
#ifndef OFFSET
#define OFFSET 0
#endif
#ifndef STRIDE
#define STRIDE 8
#endif

using Mat = Matrix<COMPONENT, 8, 8, MatrixUse::A, MatrixScope::Wave>;
groupshared STORAGE Data[COUNT];

[numthreads(64, 1, 1)]
void main() {
  // CHECK: %[[MAT:.*]] = call %dx.types.LinAlgMatrixC20M8N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC20M8N8U0S1.[[OVERLOAD]](i32 326, [[TYPE]] addrspace(3)*
  // CHECK-SAME: i32 0, i32 8, i32 0)
  // FLOAT: %[[MAT:.*]] = call %dx.types.LinAlgMatrixC9M8N8U0S1 @dx.op.linAlgMatrixLoadFromMemory.mC9M8N8U0S1.i16(i32 326, i16 addrspace(3)*
  // FLOAT-SAME: i32 4, i32 12, i32 0)
  Mat M = Mat::Load(Data, OFFSET, STRIDE, MatrixLayout::RowMajor);
  // CHECK: call void @dx.op.linAlgMatrixStoreToMemory.mC20M8N8U0S1.[[OVERLOAD]](i32 332, %dx.types.LinAlgMatrixC20M8N8U0S1 %[[MAT]], [[TYPE]] addrspace(3)*
  // CHECK-SAME: i32 0, i32 8, i32 0)
  // FLOAT: call void @dx.op.linAlgMatrixStoreToMemory.mC9M8N8U0S1.i16(i32 332, %dx.types.LinAlgMatrixC9M8N8U0S1 %[[MAT]], i16 addrspace(3)*
  // FLOAT-SAME: i32 4, i32 12, i32 0)
  M.Store(Data, OFFSET, STRIDE, MatrixLayout::RowMajor);
}
