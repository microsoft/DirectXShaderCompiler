// Run: %dxc -T vs_6_0 -E main

// CHECK:      OpName %type_TextureBuffer_T "type.TextureBuffer.T"
// CHECK-NEXT: OpMemberName %type_TextureBuffer_T 0 "a"
// CHECK-NEXT: OpMemberName %type_TextureBuffer_T 1 "b"
// CHECK-NEXT: OpMemberName %type_TextureBuffer_T 2 "c"
// CHECK-NEXT: OpMemberName %type_TextureBuffer_T 3 "d"
// CHECK-NEXT: OpMemberName %type_TextureBuffer_T 4 "s"
// CHECK-NEXT: OpMemberName %type_TextureBuffer_T 5 "t"

// CHECK:      OpName %MyTbuffer "MyTbuffer"
// CHECK:      OpName %AnotherTBuffer "AnotherTBuffer"

struct S {
  float  f1;
  float3 f2;
};

// CHECK: %type_TextureBuffer_T = OpTypeStruct %bool %int %v2uint %mat3v4float %S %_arr_float_uint_4
// CHECK: %_ptr_Uniform_type_TextureBuffer_T = OpTypePointer Uniform %type_TextureBuffer_T
struct T {
  bool     a;
  int      b;
  uint2    c;
  float3x4 d;
  S        s;
  float    t[4];
};

// CHECK: %MyTbuffer = OpVariable %_ptr_Uniform_type_TextureBuffer_T Uniform
TextureBuffer<T> MyTbuffer : register(t1);

// CHECK: %AnotherTBuffer = OpVariable %_ptr_Uniform_type_TextureBuffer_T Uniform
TextureBuffer<T> AnotherTBuffer : register(t2);

void main() {
}
