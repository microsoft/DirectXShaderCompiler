// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DOU=0 -DOTY=float16_t -DIU=0 -DITY=float16_t -DII=F16 -DMI=F16 -DML=RowMajor -DMT=0 -DBI=F16 | FileCheck %s --check-prefixes DXIL,DXIL-0
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DOU=0 -DOTY=float16_t -DIU=0 -DITY=float16_t -DII=F8_E4M3 -DMI=F8_E4M3 -DML=ColumnMajor -DMT=1 -DBI=F16 | FileCheck %s --check-prefixes DXIL,DXIL-1
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DOU=0 -DOTY=float16_t -DIU=0 -DITY=float16_t -DII=F8_E5M2 -DMI=F8_E5M2 -DML=MulOptimal -DMT=0 -DBI=F16 | FileCheck %s --check-prefixes DXIL,DXIL-2
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DOU=0 -DOTY=int -DIU=0 -DITY=int -DII=I8 -DMI=I8 -DML=OuterProductOptimal -DMT=1 -DBI=I32 | FileCheck %s --check-prefixes DXIL,DXIL-3
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DOU=0 -DOTY=int -DIU=0 -DITY=float -DII=I8 -DMI=I8 -DML=RowMajor -DMT=0 -DBI=I32 | FileCheck %s --check-prefixes DXIL,DXIL-4
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DOU=1 -DOTY=uint -DIU=0 -DITY=float -DII=I8 -DMI=F16 -DML=RowMajor -DMT=0 -DBI=I8 | FileCheck %s --check-prefixes DXIL,DXIL-5
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DOU=0 -DOTY=int -DIU=1 -DITY=uint -DII=U8 -DMI=I8 -DML=ColumnMajor -DMT=1 -DBI=I8 | FileCheck %s --check-prefixes DXIL,DXIL-6
// RUN: %dxc -T cs_6_9 %s -enable-16bit-types -DOU=0 -DOTY=int -DIU=0 -DITY=int -DII=U8 -DMI=U8 -DML=MulOptimal -DMT=0 -DBI=I8 | FileCheck %s --check-prefixes DXIL,DXIL-7

// Test minimum support set of combinations for matVecMul
// DXIL: define void @main()
// DXIL-0: call <4 x half> @dx.op.matVecMulAdd.v4f16.v8f16(i32 306, <8 x half> {{[^ ]+}}, i1 false, i32 8, %dx.types.Handle {{[^ ]+}}, i32 0, i32 8, i32 8, i32 8, i32 0, i1 false, i32 64, %dx.types.Handle {{[^ ]+}}, i32 0, i32 8, i1 false)  ; MatVecMulAdd(inputVector,isInputUnsigned,inputInterpretation,matrixBuffer,matrixOffset,matrixIntepretation,matrixM,matrixK,matrixLayout,matrixTranspose,matrixStride,biasBuffer,biasOffset,biasIntepretation,isOutputUnsigned)
// DXIL-1: call <4 x half> @dx.op.matVecMulAdd.v4f16.v8f16(i32 306, <8 x half> {{[^ ]+}}, i1 false, i32 21, %dx.types.Handle {{[^ ]+}}, i32 0, i32 21, i32 8, i32 8, i32 1, i1 true, i32 64, %dx.types.Handle {{[^ ]+}}, i32 0, i32 8, i1 false)  ; MatVecMulAdd(inputVector,isInputUnsigned,inputInterpretation,matrixBuffer,matrixOffset,matrixIntepretation,matrixM,matrixK,matrixLayout,matrixTranspose,matrixStride,biasBuffer,biasOffset,biasIntepretation,isOutputUnsigned)
// DXIL-2: call <4 x half> @dx.op.matVecMulAdd.v4f16.v8f16(i32 306, <8 x half> {{[^ ]+}}, i1 false, i32 22, %dx.types.Handle {{[^ ]+}}, i32 0, i32 22, i32 8, i32 8, i32 2, i1 false, i32 64, %dx.types.Handle {{[^ ]+}}, i32 0, i32 8, i1 false)  ; MatVecMulAdd(inputVector,isInputUnsigned,inputInterpretation,matrixBuffer,matrixOffset,matrixIntepretation,matrixM,matrixK,matrixLayout,matrixTranspose,matrixStride,biasBuffer,biasOffset,biasIntepretation,isOutputUnsigned)
// DXIL-3: call <4 x i32> @dx.op.matVecMulAdd.v4i32.v8i32(i32 306, <8 x i32> {{[^ ]+}}, i1 false, i32 20, %dx.types.Handle {{[^ ]+}}, i32 0, i32 20, i32 8, i32 8, i32 3, i1 true, i32 64, %dx.types.Handle {{[^ ]+}}, i32 0, i32 4, i1 false)  ; MatVecMulAdd(inputVector,isInputUnsigned,inputInterpretation,matrixBuffer,matrixOffset,matrixIntepretation,matrixM,matrixK,matrixLayout,matrixTranspose,matrixStride,biasBuffer,biasOffset,biasIntepretation,isOutputUnsigned)
// DXIL-4: call <4 x i32> @dx.op.matVecMulAdd.v4i32.v8f32(i32 306, <8 x float> {{[^ ]+}}, i1 false, i32 20, %dx.types.Handle {{[^ ]+}}, i32 0, i32 20, i32 8, i32 8, i32 0, i1 false, i32 64, %dx.types.Handle {{[^ ]+}}, i32 0, i32 4, i1 false)  ; MatVecMulAdd(inputVector,isInputUnsigned,inputInterpretation,matrixBuffer,matrixOffset,matrixIntepretation,matrixM,matrixK,matrixLayout,matrixTranspose,matrixStride,biasBuffer,biasOffset,biasIntepretation,isOutputUnsigned)

// Test unsigned variations
// DXIL-5: call <4 x i32> @dx.op.matVecMulAdd.v4i32.v8f32(i32 306, <8 x float> {{[^ ]+}}, i1 false, i32 20, %dx.types.Handle {{[^ ]+}}, i32 0, i32 8, i32 8, i32 8, i32 0, i1 false, i32 64, %dx.types.Handle {{[^ ]+}}, i32 0, i32 20, i1 true)  ; MatVecMulAdd(inputVector,isInputUnsigned,inputInterpretation,matrixBuffer,matrixOffset,matrixIntepretation,matrixM,matrixK,matrixLayout,matrixTranspose,matrixStride,biasBuffer,biasOffset,biasIntepretation,isOutputUnsigned)
// DXIL-6: call <4 x i32> @dx.op.matVecMulAdd.v4i32.v8i32(i32 306, <8 x i32> {{[^ ]+}}, i1 true, i32 19, %dx.types.Handle {{[^ ]+}}, i32 0, i32 20, i32 8, i32 8, i32 1, i1 true, i32 64, %dx.types.Handle {{[^ ]+}}, i32 0, i32 20, i1 false)  ; MatVecMulAdd(inputVector,isInputUnsigned,inputInterpretation,matrixBuffer,matrixOffset,matrixIntepretation,matrixM,matrixK,matrixLayout,matrixTranspose,matrixStride,biasBuffer,biasOffset,biasIntepretation,isOutputUnsigned)
// DXIL-7: call <4 x i32> @dx.op.matVecMulAdd.v4i32.v8i32(i32 306, <8 x i32> {{[^ ]+}}, i1 false, i32 19, %dx.types.Handle {{[^ ]+}}, i32 0, i32 19, i32 8, i32 8, i32 2, i1 false, i32 64, %dx.types.Handle {{[^ ]+}}, i32 0, i32 20, i1 false)  ; MatVecMulAdd(inputVector,isInputUnsigned,inputInterpretation,matrixBuffer,matrixOffset,matrixIntepretation,matrixM,matrixK,matrixLayout,matrixTranspose,matrixStride,biasBuffer,biasOffset,biasIntepretation,isOutputUnsigned)


ByteAddressBuffer input_vector_buffer; 
ByteAddressBuffer matrix_buffer;
ByteAddressBuffer bias_buffer;
RWByteAddressBuffer rw_matrix_buffer;

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

[NumThreads(1,1,1)]
void main()
{	
	vector<OTY, 4> output_vector;
	static const uint is_output_unsigned = OU;
	
	vector<ITY, 8> input_vector = input_vector_buffer.Load<vector<ITY, 8> >(0);
	const uint is_input_unsigned = IU;
	const uint input_interpretation = II;
	
	const uint matrix_offset = 0;
	const uint matrix_interpretation = MI;
	const uint matrix_dimM = 8;
	const uint matrix_dimK = 8;
	const uint matrix_layout = ML;
	const bool matrix_is_transposed = (bool) MT; 
	const uint matrix_stride = 64;

	const uint bias_offset = 0;
	const uint bias_interpretation = BI;

	__builtin_MatVecMulAdd(output_vector, is_output_unsigned, input_vector, is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset, matrix_interpretation, 
		matrix_dimM, matrix_dimK, matrix_layout, matrix_is_transposed, matrix_stride, bias_buffer, bias_offset, bias_interpretation);
}
