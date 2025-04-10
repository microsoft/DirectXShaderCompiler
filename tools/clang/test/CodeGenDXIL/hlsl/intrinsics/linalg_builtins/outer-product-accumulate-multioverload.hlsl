// RUN: %dxc -T cs_6_9 %s | FileCheck %s

RWByteAddressBuffer matrix_buffer;

// CHECK: define void @main()
// CHECK: call void @dx.op.outerProductAccumulate.v2i32.v4i32(i32 {{[0-9]+}}

[Numthreads(1,1,1)]
void main()
{
	vector<uint, 2> input_vector1 = 1;
	vector<uint, 4> input_vector2 = 2;

	const uint matrix_offset = 0;
	const uint matrix_interpretation = 5; /*U32*/
	const uint matrix_layout = 0;
	const uint matrix_stride = 64;

	__builtin_OuterProductAccumulate(input_vector1, input_vector2, matrix_buffer, matrix_offset, matrix_interpretation, matrix_layout, matrix_stride);

}
