// RUN: %dxc -T cs_6_2 -E main -spirv -fcgl -enable-16bit-types %s | FileCheck %s

struct S1 {
  uint64_t f1 : 32;
  uint64_t f2 : 1;
};
// CHECK-DAG: %S1 = OpTypeStruct %ulong

struct S2 {
  uint16_t f1 : 4;
  uint16_t f2 : 5;
};
// CHECK-DAG: %S2 = OpTypeStruct %ushort

struct S3 {
  uint64_t f1 : 45;
  uint64_t f2 : 10;
  uint16_t f3 : 7;
  uint32_t f4 : 5;
};
// CHECK-DAG: %S3 = OpTypeStruct %ulong %ushort %uint

struct S4 {
  int64_t f1 : 32;
  int64_t f2 : 1;
};
// CHECK-DAG: %S4 = OpTypeStruct %long

[numthreads(1, 1, 1)]
void main() {
  S1 s1;
  s1.f1 = 3;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_ulong %s1 %int_0
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %ulong [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ulong %ulong_4294967295 %uint_0
// CHECK: [[imask:%[0-9]+]] = OpNot %ulong [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %ulong [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %ulong %ulong_3
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %ulong [[val]] %uint_0
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %ulong [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %ulong [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]

  s1.f2 = 1;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_ulong %s1 %int_0
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %ulong [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ulong %ulong_1 %uint_32
// CHECK: [[imask:%[0-9]+]] = OpNot %ulong [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %ulong [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %ulong %ulong_1
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %ulong [[val]] %uint_32
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %ulong [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %ulong [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]

  S2 s2;
  s2.f1 = 2;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_ushort %s2 %int_0
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %ushort [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ushort %ushort_15 %uint_0
// CHECK: [[imask:%[0-9]+]] = OpNot %ushort [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %ushort [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %ushort %ushort_2
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %ushort [[val]] %uint_0
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %ushort [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %ushort [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]

  s2.f2 = 3;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_ushort %s2 %int_0
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %ushort [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ushort %ushort_31 %uint_4
// CHECK: [[imask:%[0-9]+]] = OpNot %ushort [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %ushort [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %ushort %ushort_3
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %ushort [[val]] %uint_4
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %ushort [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %ushort [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]

  S3 s3;
  s3.f1 = 5;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_ulong %s3 %int_0
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %ulong [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ulong %ulong_35184372088831 %uint_0
// CHECK: [[imask:%[0-9]+]] = OpNot %ulong [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %ulong [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %ulong %ulong_5
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %ulong [[val]] %uint_0
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %ulong [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %ulong [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]

  s3.f2 = 6;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_ulong %s3 %int_0
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %ulong [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ulong %ulong_1023 %uint_45
// CHECK: [[imask:%[0-9]+]] = OpNot %ulong [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %ulong [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %ulong %ulong_6
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %ulong [[val]] %uint_45
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %ulong [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %ulong [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]

  s3.f3 = 7;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_ushort %s3 %int_1
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %ushort [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ushort %ushort_127 %uint_0
// CHECK: [[imask:%[0-9]+]] = OpNot %ushort [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %ushort [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %ushort %ushort_7
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %ushort [[val]] %uint_0
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %ushort [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %ushort [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]

  s3.f4 = 8;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_uint %s3 %int_2
// CHECK:   [[val:%[0-9]+]] = OpLoad %uint [[ptr]]
// CHECK:   [[tmp:%[0-9]+]] = OpBitFieldInsert %uint [[val]] %uint_8 %uint_0 %uint_5
// CHECK:                     OpStore [[ptr]] [[tmp]]

  S4 s4;
  s4.f1 = 3;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_long %s4 %int_0
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %long [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ulong %ulong_4294967295 %uint_0
// CHECK: [[imask:%[0-9]+]] = OpNot %ulong [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %long [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %long %long_3
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %long [[val]] %uint_0
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %long [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %long [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]

  s4.f2 = 1;
// CHECK:   [[ptr:%[0-9]+]] = OpAccessChain %_ptr_Function_long %s4 %int_0
// CHECK:   [[tmp:%[0-9]+]] = OpLoad %long [[ptr]]
// CHECK:  [[mask:%[0-9]+]] = OpShiftLeftLogical %ulong %ulong_1 %uint_32
// CHECK: [[imask:%[0-9]+]] = OpNot %ulong [[mask]]
// CHECK:   [[dst:%[0-9]+]] = OpBitwiseAnd %long [[tmp]] [[imask]]
// CHECK:   [[val:%[0-9]+]] = OpBitcast %long %long_1
// CHECK:   [[tmp:%[0-9]+]] = OpShiftLeftLogical %long [[val]] %uint_32
// CHECK:   [[src:%[0-9]+]] = OpBitwiseAnd %long [[tmp]] [[mask]]
// CHECK:   [[mix:%[0-9]+]] = OpBitwiseOr %long [[dst]] [[src]]
// CHECK:                     OpStore [[ptr]] [[mix]]
}
