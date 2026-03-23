// RUN: %dxc -T ps_6_0 -E main %s -spirv -Zpc | FileCheck %s -check-prefix=COL -check-prefix=CHECK
// RUN: %dxc -T ps_6_0 -E main %s -spirv -Zpr | FileCheck %s -check-prefix=ROW -check-prefix=CHECK

struct S {
  float2x3 a;
};

struct T {
  float a[6];
};

RWStructuredBuffer<S> s_output;
RWStructuredBuffer<T> t_output;

// See https://github.com/microsoft/DirectXShaderCompiler/blob/438781364eea22d188b337be1dfa4174ed7d928c/docs/SPIR-V.rst#L723.
// COL: OpMemberDecorate %S 0 RowMajor
// ROW: OpMemberDecorate %S 0 ColMajor

// The DXIL that is generates different order for the values depending on
// whether the matrix is column or row major. However, for SPIR-V, the value
// stored in both cases is the same because the decoration, which is checked
// above, is what determines the layout in memory for the value.

// CHECK: [[mat:%[0-9]+]] = OpLoad %mat2v3float
// CHECK: [[e00:%[0-9]+]] = OpCompositeExtract %float [[mat]] 0 0
// CHECK: [[e01:%[0-9]+]] = OpCompositeExtract %float [[mat]] 0 1
// CHECK: [[e02:%[0-9]+]] = OpCompositeExtract %float [[mat]] 0 2
// CHECK: [[e10:%[0-9]+]] = OpCompositeExtract %float [[mat]] 1 0
// CHECK: [[e11:%[0-9]+]] = OpCompositeExtract %float [[mat]] 1 1
// CHECK: [[e12:%[0-9]+]] = OpCompositeExtract %float [[mat]] 1 2

void main() {
  S s;
  [unroll]
  for( int i = 0; i < 2; ++i) {
    [unroll]
    for( int j = 0; j < 3; ++j) {
      s.a[i][j] = i*3+j;
    }
  }
// CHECK: [[tptr:%[0-9]+]] = OpAccessChain %_ptr_Uniform_T %t_output %int_0 %uint_0
// CHECK: [[tarr:%[0-9]+]] = OpCompositeConstruct %_arr_float_uint_6 [[e00]] [[e01]] [[e02]] [[e10]] [[e11]] [[e12]]
// CHECK: [[tval:%[0-9]+]] = OpCompositeConstruct %T [[tarr]]
// CHECK: OpStore [[tptr]] [[tval]]
  T t = (T)(s);
  t_output[0] = t;

// CHECK: [[sptr:%[0-9]+]] = OpAccessChain %_ptr_Uniform_S %s_output %int_0 %uint_0
// CHECK: [[sval:%[0-9]+]] = OpCompositeConstruct %S [[mat]]
// CHECK: OpStore [[sptr]] [[sval]]
  s = (S)t;
  s_output[0] = s;
}
