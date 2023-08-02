// RUN: %dxc -E main -T vs_6_2 -HV 2018 -enable-16bit-types

struct Empty {};

AppendStructuredBuffer<int> buf;

void main() {
  // Test size and packing of scalar types, vectors and arrays all at once.

  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_12
  buf.Append(sizeof(int16_t3[2]));
  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_12
  buf.Append(sizeof(half3[2]));

  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_24
  buf.Append(sizeof(int3[2]));
  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_24
  buf.Append(sizeof(float3[2]));
  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_24
  buf.Append(sizeof(bool3[2]));

  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_48
  buf.Append(sizeof(int64_t3[2]));
  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_48
  buf.Append(sizeof(double3[2]));

  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_0
  buf.Append(sizeof(Empty[2]));

  // CHECK:      [[buf:%\d+]] = OpAccessChain %_ptr_Uniform_int %buf %uint_0
  // CHECK-NEXT:                OpStore [[buf]] %int_8
  struct
  {
    int16_t i16;
    // 2-byte padding
    struct { float f32; } s; // Nested type
    struct {} _; // Zero-sized field.
  } complexStruct;
  buf.Append(sizeof(complexStruct));

// CHECK:         [[foo:%\d+]] = OpLoad %int %foo
// CHECK-NEXT: [[ui_foo:%\d+]] = OpBitcast %uint [[foo]]
// CHECK-NEXT:                   OpIMul %uint %uint_4 [[ui_foo]]
  int foo;
  buf.Append(sizeof(float) * foo);
}
