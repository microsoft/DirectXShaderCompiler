// Run: %dxc -T vs_6_0 -E main

// NOTE: According to Item "Data rules" of SPIR-V Spec 2.16.1 "Universal
// Validation Rules":
//   Matrix types can only be parameterized with floating-point types.
//
// So we need special handling for matrices with non-fp elements. An extension
// to SPIR-V to lessen the above rule is a possible way, which will enable the
// generation of SPIR-V currently commented out. Or we can emulate them using
// other types.

void main() {
// XXXXX: %int = OpTypeInt 32 1
// XXXXX: %uint = OpTypeInt 32 0

// CHECK: %float = OpTypeFloat 32
    float1x1 mat11;
// XXXXX: %v2int = OpTypeVector %int 2
    //int1x2   mat12;
// XXXXX: %v3uint = OpTypeVector %uint 3
    //uint1x3  mat13;
// XXXXX: %bool = OpTypeBool
// XXXXX-NEXT: %v4bool = OpTypeVector %bool 4
    //bool1x4  mat14;

    //int2x1   mat21;
// XXXXX: %v2uint = OpTypeVector %uint 2
// XXXXX-NEXT: %mat2v2uint = OpTypeMatrix %v2uint 2
    //uint2x2  mat22;
// XXXXX: %v3bool = OpTypeVector %bool 3
// XXXXX-NEXT: %mat2v3bool = OpTypeMatrix %v3bool 2
    //bool2x3  mat23;
// CHECK: %v4float = OpTypeVector %float 4
// CHECK-NEXT: %mat2v4float = OpTypeMatrix %v4float 2
    float2x4 mat24;

    //uint3x1  mat31;
// XXXXX: %v2bool = OpTypeVector %bool 2
// XXXXX-NEXT: %mat3v2bool = OpTypeMatrix %v2bool 3
    //bool3x2  mat32;
// CHECK: %v3float = OpTypeVector %float 3
// CHECK-NEXT: %mat3v3float = OpTypeMatrix %v3float 3
    float3x3 mat33;
// XXXXX: %v4int = OpTypeVector %int 4
// XXXXX-NEXT: %mat3v4int = OpTypeMatrix %v4int 3
    //int3x4   mat34;

    //bool4x1  mat41;
// CHECK: %v2float = OpTypeVector %float 2
// CHECK-NEXT: %mat4v2float = OpTypeMatrix %v2float 4
    float4x2 mat42;
// XXXXX: %v3int = OpTypeVector %int 3
// XXXXX-NEXT: %mat4v3int = OpTypeMatrix %v3int 4
    //int4x3   mat43;
// XXXXX: %v4uint = OpTypeVector %uint 4
// XXXXX-NEXT: %mat4v4uint = OpTypeMatrix %v4uint 4
    //uint4x4  mat44;

// CHECK: %mat4v4float = OpTypeMatrix %v4float 4
    matrix mat;

    //matrix<int, 1, 1>   imat11;
    //matrix<uint, 1, 3>  umat23;
    matrix<float, 2, 1> fmat21;
    matrix<float, 1, 2> fmat12;
// XXXXX: %mat3v4bool = OpTypeMatrix %v4bool 3
    //matrix<bool, 3, 4>  bmat34;

// CHECK-LABEL: %bb_entry = OpLabel


// CHECK-NEXT: %mat11 = OpVariable %_ptr_Function_float Function
// XXXXX-NEXT: %mat12 = OpVariable %_ptr_Function_v2int Function
// XXXXX-NEXT: %mat13 = OpVariable %_ptr_Function_v3uint Function
// XXXXX-NEXT: %mat14 = OpVariable %_ptr_Function_v4bool Function

// XXXXX-NEXT: %mat21 = OpVariable %_ptr_Function_v2int Function
// XXXXX-NEXT: %mat22 = OpVariable %_ptr_Function_mat2v2uint Function
// XXXXX-NEXT: %mat23 = OpVariable %_ptr_Function_mat2v3bool Function
// CHECK-NEXT: %mat24 = OpVariable %_ptr_Function_mat2v4float Function

// XXXXX-NEXT: %mat31 = OpVariable %_ptr_Function_v3uint Function
// XXXXX-NEXT: %mat32 = OpVariable %_ptr_Function_mat3v2bool Function
// CHECK-NEXT: %mat33 = OpVariable %_ptr_Function_mat3v3float Function
// XXXXX-NEXT: %mat34 = OpVariable %_ptr_Function_mat3v4int Function

// XXXXX-NEXT: %mat41 = OpVariable %_ptr_Function_v4bool Function
// CHECK-NEXT: %mat42 = OpVariable %_ptr_Function_mat4v2float Function
// XXXXX-NEXT: %mat43 = OpVariable %_ptr_Function_mat4v3int Function
// XXXXX-NEXT: %mat44 = OpVariable %_ptr_Function_mat4v4uint Function

// CHECK-NEXT: %mat = OpVariable %_ptr_Function_mat4v4float Function

// XXXXX-NEXT: %imat11 = OpVariable %_ptr_Function_int Function
// XXXXX-NEXT: %umat23 = OpVariable %_ptr_Function_v3uint Function
// CHECK-NEXT: %fmat21 = OpVariable %_ptr_Function_v2float Function
// CHECK-NEXT: %fmat12 = OpVariable %_ptr_Function_v2float Function
// XXXXX-NEXT: %bmat34 = OpVariable %_ptr_Function_mat3v4bool Function
}
