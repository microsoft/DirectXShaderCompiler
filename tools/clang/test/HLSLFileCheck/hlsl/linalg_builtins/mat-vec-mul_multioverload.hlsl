// RUN: %dxc -T cs_6_9 %s | FileCheck %s

ByteAddressBuffer input_vector_buffer; 
ByteAddressBuffer matrix_buffer;
ByteAddressBuffer bias_buffer;
RWByteAddressBuffer rw_matrix_buffer;

// Test use of __builtin_MatVecMulAdd in compute shader
// CHECK: define void @main()
// CHECK:   call <4 x i32> @dx.op.matVecMul.v4i32.v8f32(i32 {{[0-9]+}}


[NumThreads(1,1,1)]
void main()
{	
	vector<uint32_t, 4> output_vector;
	static const uint is_output_unsigned = 0;
	
	vector<float, 8> input_vector = input_vector_buffer.Load<vector<float, 8> >(0);
	const uint is_input_unsigned = 0;
	const uint input_interpretation = 9; /*F32*/
	
	const uint matrix_offset = 0;
	const uint matrix_interpretation = 9; /*F32*/
	const uint matrix_dimM = 8;
	const uint matrix_dimK = 8;
	const uint matrix_layout = 0; /*RowMajor*/
	const bool matrix_is_transposed = false; 
	const uint matrix_stride = 64;

	__builtin_MatVecMul(output_vector, is_output_unsigned, input_vector, is_input_unsigned, input_interpretation, matrix_buffer, matrix_offset, matrix_interpretation, 
		matrix_dimM, matrix_dimK, matrix_layout, matrix_is_transposed, matrix_stride);
}
