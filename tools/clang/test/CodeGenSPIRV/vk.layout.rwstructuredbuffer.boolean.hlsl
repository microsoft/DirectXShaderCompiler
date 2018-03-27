// Run: %dxc -T vs_6_0 -E main

struct S
{
  bool  boolScalar;
  bool3 boolVec;
  row_major bool2x3 boolMat;
};

RWStructuredBuffer<S> values;

// CHECK: [[v3uint1:%\d+]] = OpConstantComposite %v3uint %uint_1 %uint_1 %uint_1
// CHECK: [[v3uint0:%\d+]] = OpConstantComposite %v3uint %uint_0 %uint_0 %uint_0
void main()
{
  bool3 boolVecVar;
  bool2 boolVecVar2;
  row_major bool2x3 boolMatVar;

// CHECK:                [[uintPtr:%\d+]] = OpAccessChain %_ptr_Uniform_uint %values %int_0 %uint_0 %int_0
// CHECK-NEXT: [[convertTrueToUint:%\d+]] = OpSelect %uint %true %uint_1 %uint_0
// CHECK-NEXT:                              OpStore [[uintPtr]] [[convertTrueToUint]]
  values[0].boolScalar = true;

// CHECK:      [[boolVecVar:%\d+]] = OpLoad %v3bool %boolVecVar
// CHECK-NEXT: [[uintVecPtr:%\d+]] = OpAccessChain %_ptr_Uniform_v3uint %values %int_0 %uint_1 %int_1
// CHECK-NEXT: [[uintVecVar:%\d+]] = OpSelect %v3uint [[boolVecVar]] [[v3uint1]] [[v3uint0]]
// CHECK-NEXT:                       OpStore [[uintVecPtr]] [[uintVecVar]]
  values[1].boolVec = boolVecVar;

  // TODO: In the following cases, OpAccessChain runs into type mismatch issues due to decoration differences.
  // values[2].boolMat = boolMatVar;
  // values[0].boolVec.yzx = boolVecVar;
  // values[0].boolMat._m12_m11 = boolVecVar2;
}
