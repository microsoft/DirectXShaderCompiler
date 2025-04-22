// RUN: %dxc -I %hlsl_headers -T lib_6_9 -enable-16bit-types %s | FileCheck %s

#include <dx/linalg.h>

ByteAddressBuffer Buf;

export float4 Test1(vector<float, 4> Input) {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_FLOAT16, 4, 4, MATRIX_LAYOUT_MUL_OPTIMAL, true> Matrix = {
      Buf, 0, 0};

  // CHECK: %{{.+}} = call <4 x float> @dx.op.matVecMul.v4f32.v4f32(i32 305, <4 x float> %{{.+}}, i1 false, i32 8, %dx.types.Handle %{{.+}}, i32 0, i32 8, i32 4, i32 4, i32 2, i1 true, i32 0, i1 false)
  return Mul<float>(    
      Matrix, MakeInterpretedVector<DATA_TYPE_FLOAT16>(Input));
}

export vector<float, 8> Test2(vector<uint8_t4_packed, 6> Input) {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_UINT8, 8, 6 * 4, MATRIX_LAYOUT_MUL_OPTIMAL> Matrix = {
      Buf, 0, 0};

  // note the stride argument is dropped.
  // CHECK: %{{.+}} = call <8 x float> @dx.op.matVecMul.v8f32.v6f32(i32 305, <6 x float> %{{.+}}, i1 false, i32 18, %dx.types.Handle %{{.+}}, i32 0, i32 19, i32 8, i32 24, i32 2, i1 false, i32 0, i1 false)
  return Mul<float>(Matrix,
                    MakeInterpretedVector<DATA_TYPE_UINT8_T4_PACKED>(Input));
}

// test that "stride" isn't ignored in non-optimal layouts
export vector<float, 8> Test3(vector<uint8_t4_packed, 6> Input) {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_UINT8, 8, 6 * 4, MATRIX_LAYOUT_ROW_MAJOR> Matrix = {
      Buf, 0, 6 * 4 * 8};

  // CHECK: %{{.+}} = call <8 x float> @dx.op.matVecMul.v8f32.v6f32(i32 305, <6 x float> %{{.+}}, i1 false, i32 18, %dx.types.Handle %{{.+}}, i32 0, i32 19, i32 8, i32 24, i32 0, i1 false, i32 192, i1 false)
  return Mul<float>(Matrix,
                    MakeInterpretedVector<DATA_TYPE_UINT8_T4_PACKED>(Input));
}
