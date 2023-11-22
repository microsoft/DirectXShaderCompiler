// RUN: %dxc -T cs_6_0 -E main -Zpr

// CHECK: %SData = OpTypeStruct %_arr_mat3v4float_uint_2 %_arr_mat3v4float_uint_2_0
struct SData {
                float3x4 mat1[2];
   column_major float3x4 mat2[2];
};

// CHECK: %type_SBufferData = OpTypeStruct %SData %_arr_mat3v4float_uint_2 %_arr_mat3v4float_uint_2_0 %mat3v4float
cbuffer SBufferData {
                SData    BufferData;
                float3x4 Mat1[2];
   column_major float3x4 Mat2[2];
   row_major    float3x4 Mat3;
};

// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_SData %SBufferData %int_0
// CHECK: [[val:%\d+]] = OpLoad %SData [[ptr]]
// CHECK:     {{%\d+}} = OpCompositeExtract %_arr_mat3v4float_uint_2 [[val]] 0
// CHECK:     {{%\d+}} = OpCompositeExtract %_arr_mat3v4float_uint_2_0 [[val]] 1
static const SData Data = BufferData;

// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_SData %SBufferData %int_0
// CHECK:     {{%\d+}} = OpAccessChain %_ptr_Uniform__arr_mat3v4float_uint_2 [[ptr]] %int_0
static const float3x4 Matrices[2] = BufferData.mat1;

// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_SData %SBufferData %int_0
// CHECK:     {{%\d+}} = OpAccessChain %_ptr_Uniform_mat3v4float [[ptr]] %int_1 %int_1
static const float3x4 Matrix = BufferData.mat2[1];

RWStructuredBuffer<float4> Out;

[numthreads(4, 4, 4)]
void main() {
// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform__arr_mat3v4float_uint_2 %SBufferData %int_1
// CHECK:     {{%\d+}} = OpLoad %_arr_mat3v4float_uint_2 [[ptr]]
  float3x4 a[2] = Mat1;
// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform__arr_mat3v4float_uint_2_0 %SBufferData %int_2
// CHECK:     {{%\d+}} = OpLoad %_arr_mat3v4float_uint_2_0 [[ptr]]
  float3x4 b[2] = Mat2;

// CHECK: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform__arr_mat3v4float_uint_2_0 %SBufferData %int_2
// CHECK:     {{%\d+}} = OpAccessChain %_ptr_Uniform_mat3v4float [[ptr]] %int_1
  float3x4 c = Mat2[1];
// CHECK:     {{%\d+}} = OpAccessChain %_ptr_Uniform_mat3v4float %SBufferData %int_3
  float3x4 d = Mat3;

  Out[0] = Data.mat1[0][0];
}
