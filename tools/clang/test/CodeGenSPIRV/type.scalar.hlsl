// Run: %dxc -T ps_6_0 -E main

// TODO
// - 16bit & 64bit integers (require additional capabilities)
// - 16bit         floats   (require additional capabilities)

// CHECK: OpCapability Float64

// CHECK-DAG: %void = OpTypeVoid
// CHECK-DAG: %{{[0-9]+}} = OpTypeFunction %void
void main() {
// CHECK-DAG: %bool = OpTypeBool
// CHECK-DAG: %_ptr_Function_bool = OpTypePointer Function %bool
  bool a;
// CHECK-DAG: %int = OpTypeInt 32 1
// CHECK-DAG: %_ptr_Function_int = OpTypePointer Function %int
  int b;
// CHECK-DAG: %uint = OpTypeInt 32 0
// CHECK-DAG: %_ptr_Function_uint = OpTypePointer Function %uint
  uint c;
  dword d;
// CHECK-DAG: %float = OpTypeFloat 32
// CHECK-DAG: %_ptr_Function_float = OpTypePointer Function %float
  float e;
  half f;
// CHECK-DAG: %double = OpTypeFloat 64
// CHECK-DAG: %_ptr_Function_double = OpTypePointer Function %double
  double g;

  bool1 a1;
  int1 b1;
  uint1 c1;
  dword1 d1;
  float1 e1;
  half1 f1;
  double1 g1;

// CHECK: %a = OpVariable %_ptr_Function_bool Function
// CHECK-NEXT: %b = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: %c = OpVariable %_ptr_Function_uint Function
// CHECK-NEXT: %d = OpVariable %_ptr_Function_uint Function
// CHECK-NEXT: %e = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %f = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %g = OpVariable %_ptr_Function_double Function
// CHECK-NEXT: %a1 = OpVariable %_ptr_Function_bool Function
// CHECK-NEXT: %b1 = OpVariable %_ptr_Function_int Function
// CHECK-NEXT: %c1 = OpVariable %_ptr_Function_uint Function
// CHECK-NEXT: %d1 = OpVariable %_ptr_Function_uint Function
// CHECK-NEXT: %e1 = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %f1 = OpVariable %_ptr_Function_float Function
// CHECK-NEXT: %g1 = OpVariable %_ptr_Function_double Function
}
