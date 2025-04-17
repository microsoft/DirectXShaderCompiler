// RUN: %dxc -I %hlsl_headers -T lib_6_9 -enable-16bit-types %s

#include "dx/linalg.h"

namespace code_example {
ByteAddressBuffer Model;

vector<float, 3> ApplyNeuralMaterial(vector<half, 8> InputVector) {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_FLOAT8_E4M3, 32, 8, MATRIX_LAYOUT_MUL_OPTIMAL> Matrix0 = {
      Model, 0, 0};

  VectorRef<DATA_TYPE_FLOAT16> BiasVector0 = {Model, 1024};

  MatrixRef<DATA_TYPE_FLOAT8_E4M3, 32, 32, MATRIX_LAYOUT_MUL_OPTIMAL> Matrix1 =
      {Model, 2048, 0};

  VectorRef<DATA_TYPE_FLOAT16> BiasVector1 = {Model, 3072};

  MatrixRef<DATA_TYPE_FLOAT8_E4M3, 3, 32, MATRIX_LAYOUT_MUL_OPTIMAL> Matrix2 = {
      Model, 4096, 0};

  VectorRef<DATA_TYPE_FLOAT16> BiasVector2 = {Model, 5120};

  vector<half, 32> Layer0 = MulAdd<half>(
      Matrix0, MakeInterpretedVector<DATA_TYPE_FLOAT8_E4M3>(InputVector),
      BiasVector0);
  Layer0 = max(Layer0, 0);

  vector<half, 32> Layer1 = MulAdd<half>(
      Matrix1, MakeInterpretedVector<DATA_TYPE_FLOAT8_E4M3>(Layer0),
      BiasVector1);
  Layer1 = max(Layer1, 0);

  vector<float, 3> Output = MulAdd<float>(
      Matrix2, MakeInterpretedVector<DATA_TYPE_FLOAT8_E4M3>(Layer1),
      BiasVector2);
  Output = exp(Output);

  return Output;
}
} // namespace code_example

namespace matrixref_example {
ByteAddressBuffer ROBuffer;
RWByteAddressBuffer RWBuffer;

void Example() {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_FLOAT16, 4, 4, MATRIX_LAYOUT_MUL_OPTIMAL, true> MatrixA =
      {ROBuffer, /*offset=*/128, /*stride=*/0};

  MatrixRef<DATA_TYPE_FLOAT16, 4, 4, MATRIX_LAYOUT_ROW_MAJOR, true> MatrixB = {
      ROBuffer, /*offset=*/128, /*stride=*/16};

  RWMatrixRef<DATA_TYPE_FLOAT16, 128, 256, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL>
      MatrixC = {RWBuffer, /*offset=*/64, /*stride=*/0};
}
} // namespace matrixref_example

namespace vectorref_example {
ByteAddressBuffer ROBuffer;
RWByteAddressBuffer RWBuffer;

void Example() {
  using namespace dx::linalg;

  VectorRef<DATA_TYPE_FLOAT16> VectorA = {ROBuffer, /*offset=*/128};
  VectorRef<DATA_TYPE_FLOAT32> VectorB = {ROBuffer, /*offset=*/128};
  RWVectorRef<DATA_TYPE_SINT16> VectorC = {RWBuffer, /*offset=*/64};
}
} // namespace vectorref_example

namespace vector_example {
ByteAddressBuffer Buffer;
void Example() {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_FLOAT16, 128, 128, MATRIX_LAYOUT_MUL_OPTIMAL, true>
      Matrix = {Buffer, 0, 0};

  vector<float, 128> V = 0;
  vector<float, 128> Result =
      Mul<float>(Matrix, MakeInterpretedVector<DATA_TYPE_FLOAT8_E4M3>(V));

  // alternative:
  InterpretedVector<float, 128, DATA_TYPE_FLOAT8_E4M3> IV = {V};
  vector<float, 128> Result2 = Mul<float>(Matrix, IV);
}
} // namespace vector_example

namespace mul_example {
ByteAddressBuffer Buffer;
float4 Example(float4 Input) {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_FLOAT16, 4, 4, MATRIX_LAYOUT_MUL_OPTIMAL, true> Matrix = {
      Buffer, 0, 0};

  return Mul<float>(Matrix, MakeInterpretedVector<DATA_TYPE_FLOAT16>(Input));
}
} // namespace mul_example

namespace muladd_example {
ByteAddressBuffer Buffer;

void Example() {
  using namespace dx::linalg;

  MatrixRef<DATA_TYPE_FLOAT8_E4M3, 32, 8, MATRIX_LAYOUT_MUL_OPTIMAL> Matrix = {
      Buffer, 0, 0};

  VectorRef<DATA_TYPE_FLOAT16> BiasVector = {Buffer, 1024};

  vector<float, 8> V = 0;
  vector<float, 32> Result = MulAdd<float>(
      Matrix, MakeInterpretedVector<DATA_TYPE_FLOAT8_E4M3>(V), BiasVector);
}
} // namespace muladd_example

namespace outerproductaccumulate_example {
RWByteAddressBuffer RWBuf;

void Example(vector<half, 128> Input1, vector<half, 256> Input2) {
  using namespace dx::linalg;

  RWMatrixRef<DATA_TYPE_FLOAT16, 128, 256, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL>
      Matrix = {RWBuf, 0, 0};

  OuterProductAccumulate(Input1, Input2, Matrix);
}
} // namespace outerproductaccumulate_example

namespace vector_accumulate {
RWByteAddressBuffer RWBuf;

void Test(vector<half, 128> Input) {
  using namespace dx::linalg;
  VectorAccumulate(Input, RWBuf, 0);
}
} // namespace vector_accumulate
