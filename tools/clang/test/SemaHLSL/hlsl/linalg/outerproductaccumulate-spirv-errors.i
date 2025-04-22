#line 1 "D:\\DXC\\tools\\clang\\test\\SemaHLSL\\hlsl\\linalg\\outerproductaccumulate-spirv-errors.hlsl"






#line 1 "D:/DXC/tools/clang/lib/Headers/hlsl\\dx/linalg.h"
#line 11 "D:/DXC/tools/clang/lib/Headers/hlsl\\dx/linalg.h"
namespace dx {
namespace linalg {





enum DataType {
  DATA_TYPE_SINT16 = 2,
  DATA_TYPE_UINT16 = 3,
  DATA_TYPE_SINT32 = 4,
  DATA_TYPE_UINT32 = 5,
  DATA_TYPE_FLOAT16 = 8,
  DATA_TYPE_FLOAT32 = 9,
  DATA_TYPE_SINT8_T4_PACKED = 17,
  DATA_TYPE_UINT8_T4_PACKED = 18,
  DATA_TYPE_UINT8 = 19,
  DATA_TYPE_SINT8 = 20,
  DATA_TYPE_FLOAT8_E4M3 = 21,

  DATA_TYPE_FLOAT8_E5M2 = 22,

};

enum MatrixLayout {
  MATRIX_LAYOUT_ROW_MAJOR = 0,
  MATRIX_LAYOUT_COLUMN_MAJOR = 1,
  MATRIX_LAYOUT_MUL_OPTIMAL = 2,
  MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL = 3
};




namespace details {
template <typename T> bool IsUnsigned() { return false; }


template <> bool IsUnsigned<uint16_t>() { return true; }


template <> bool IsUnsigned<uint32_t>() { return true; }
template <> bool IsUnsigned<uint64_t>() { return true; }
}





template <typename BufferTy, DataType DT, uint M, uint K, MatrixLayout ML,
          bool Transpose>
struct MatrixRefImpl {
  BufferTy Buffer;
  uint StartOffset;
  uint Stride;
};

template <DataType DT, uint M, uint K, MatrixLayout ML, bool Transpose = false>
using MatrixRef = MatrixRefImpl<ByteAddressBuffer, DT, M, K, ML, Transpose>;

template <DataType DT, uint M, uint K, MatrixLayout ML, bool Transpose = false>
using RWMatrixRef = MatrixRefImpl<RWByteAddressBuffer, DT, M, K, ML, Transpose>;





template <typename BufferTy, DataType DT> struct VectorRefImpl {
  BufferTy Buffer;
  uint StartOffset;
};

template <DataType DT> using VectorRef = VectorRefImpl<ByteAddressBuffer, DT>;

template <DataType DT>
using RWVectorRef = VectorRefImpl<RWByteAddressBuffer, DT>;





template <typename T, int N, DataType DT> struct InterpretedVector {
  vector<T, N> Data;
};

template <DataType DT, typename T, int N>
InterpretedVector<T, N, DT> MakeInterpretedVector(vector<T, N> Vec) {
  InterpretedVector<T, N, DT> IV = {Vec};
  return IV;
}





template <typename OutputElTy, typename InputElTy, int InputElCount,
          typename MatrixBufferTy, DataType InputDT, DataType MatrixDT,
          uint MatrixM, uint MatrixK, MatrixLayout MatrixLayout,
          bool MatrixTranspose>
vector<OutputElTy, MatrixM>
Mul(MatrixRefImpl<MatrixBufferTy, MatrixDT, MatrixM, MatrixK, MatrixLayout,
                  MatrixTranspose>
        Matrix,
    InterpretedVector<InputElTy, InputElCount, InputDT> InputVector) {

  vector<OutputElTy, MatrixM> OutputVector;

  __builtin_MatVecMul(
              OutputVector, details::IsUnsigned<OutputElTy>(), InputVector.Data,
      details::IsUnsigned<InputElTy>(), InputDT, Matrix.Buffer,
      Matrix.StartOffset, MatrixDT, MatrixM, MatrixK, MatrixLayout,
      MatrixTranspose, Matrix.Stride);

  return OutputVector;
}





template <typename OutputElTy, typename InputElTy, int InputElCount,
          typename MatrixBufferTy, DataType InputDT, DataType MatrixDT,
          uint MatrixM, uint MatrixK, MatrixLayout MatrixLayout,
          bool MatrixTranspose, typename BiasVectorBufferTy,
          DataType BiasVectorDT>
vector<OutputElTy, MatrixM>
MulAdd(MatrixRefImpl<MatrixBufferTy, MatrixDT, MatrixM, MatrixK, MatrixLayout,
                     MatrixTranspose>
           Matrix,
       InterpretedVector<InputElTy, InputElCount, InputDT> InputVector,
       VectorRefImpl<BiasVectorBufferTy, BiasVectorDT> BiasVector) {

  vector<OutputElTy, MatrixM> OutputVector;

  __builtin_MatVecMulAdd(
              OutputVector, details::IsUnsigned<OutputElTy>(), InputVector.Data,
      details::IsUnsigned<InputElTy>(), InputDT, Matrix.Buffer,
      Matrix.StartOffset, MatrixDT, MatrixM, MatrixK, MatrixLayout,
      MatrixTranspose, Matrix.Stride, BiasVector.Buffer, BiasVector.StartOffset,
      BiasVectorDT);

  return OutputVector;
}





template <typename ElTy, int MatrixM, int MatrixN, DataType MatrixDT,
          MatrixLayout MatrixLayout>
void OuterProductAccumulate(
    vector<ElTy, MatrixM> InputVector1, vector<ElTy, MatrixN> InputVector2,
    RWMatrixRef<MatrixDT, MatrixM, MatrixN, MatrixLayout, false> Matrix) {
  __builtin_OuterProductAccumulate(InputVector1, InputVector2, Matrix.Buffer,
                                   Matrix.StartOffset, MatrixDT, MatrixLayout,
                                   Matrix.Stride);
}





template <typename ElTy, int ElCount>
void VectorAccumulate(vector<ElTy, ElCount> InputVector,
                      RWByteAddressBuffer Buffer, uint Offset) {
  __builtin_VectorAccumulate(InputVector, Buffer, Offset);
}

}
}
#line 7 "D:\\DXC\\tools\\clang\\test\\SemaHLSL\\hlsl\\linalg\\outerproductaccumulate-spirv-errors.hlsl"


RWByteAddressBuffer RWBuf;

export void Test4(vector<half, 128> Input1, vector<half, 64> Input2) {
  using namespace dx::linalg;

  RWMatrixRef<DATA_TYPE_FLOAT16, 128, 64, MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL>
      matrix = {RWBuf, 0, 0};


  OuterProductAccumulate(Input1, Input2, matrix);
}
