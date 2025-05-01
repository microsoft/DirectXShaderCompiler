// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DML=RowMajor -DSTRIDE=64 2>&1| FileCheck %s --check-prefixes DXIL-0
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DML=ColumnMajor -DSTRIDE=64 2>&1 | FileCheck %s --check-prefixes DXIL-1
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DML=MulOptimal -DSTRIDE=64 2>&1 | FileCheck %s --check-prefixes DXIL-2
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DML=OuterProductOptimal -DSTRIDE=64 2>&1 | FileCheck %s --check-prefixes DXIL-3
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DML=OuterProductOptimal -DSTRIDE=0 2>&1 | FileCheck %s --check-prefixes DXIL-4
ByteAddressBuffer input_vector_buffer;
ByteAddressBuffer input_vector_buffer2;
RWByteAddressBuffer matrix_buffer;

enum CompType {
  Invalid = 0,
  I1 = 1,
  I16 = 2,
  U16 = 3,
  I32 = 4,
  U32 = 5,
  I64 = 6,
  U64 = 7,
  F16 = 8,
  F32 = 9,
  F64 = 10,
  SNormF16 = 11,
  UNormF16 = 12,
  SNormF32 = 13,
  UNormF32 = 14,
  SNormF64 = 15,
  UNormF64 = 16,
  PackedS8x32 = 17,
  PackedU8x32 = 18,

  // BEGIN NEW FOR SM 6.9
  U8 = 19,
  I8 = 20,
  F8_E4M3 = 21,
  F8_E5M2 = 22,
};

enum MatLayout {
  RowMajor = 0,
  ColumnMajor = 1,
  MulOptimal = 2,
  OuterProductOptimal = 3,
};

// DXIL-0: error: matrix layout value 'RowMajor' is not valid for outerproductaccumulate, must be 'OuterProductOptimal'
// DXIL-1: error: matrix layout value 'ColumnMajor' is not valid for outerproductaccumulate, must be 'OuterProductOptimal' 
// DXIL-2: error: matrix layout value 'MulOptimal' is not valid for outerproductaccumulate, must be 'OuterProductOptimal' 
// DXIL-3-NOT: error: matrix layout value 'OuterProductOptimal' is not valid for outerproductaccumulate, must be 'OuterProductOptimal' 
// DXIL-3: error: matrix stride must be zero for optimal layouts 
// DXIL-4: call void @dx.op.outerProductAccumulate.v8f16.v8f16(i32 307, <8 x half> %{{[^ ]+}}, <8 x half> %{{[^ ]+}}, %dx.types.Handle %{{[^ ]+}}, i32 0, i32 8, i32 3, i32 0)

[Numthreads(1,1,1)]
[shader("compute")]
void main()
{
    vector<half, 8> input_vector1 = input_vector_buffer.Load<vector<half, 8> >(0);
    vector<half, 8> input_vector2 = input_vector_buffer2.Load<vector<half, 8> >(0);

    const uint matrix_interpretation = CompType::F16;
    const uint matrix_layout = ML;
    const uint matrix_offset = 0;
    const uint matrix_stride = STRIDE;
    
    // CHECK: matrix layout value 'RowMajor' is not valid for outerproductaccumulate, must be 'OuterProductOptimal' 
    __builtin_OuterProductAccumulate(input_vector1, input_vector2, matrix_buffer, matrix_offset, matrix_interpretation, matrix_layout, matrix_stride);

}
