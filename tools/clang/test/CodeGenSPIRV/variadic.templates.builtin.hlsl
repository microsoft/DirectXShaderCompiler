// RUN: %dxc -E main -T ps_6_0 -HV 202x -fcgl %s -spirv | FileCheck %s

// Verify pack expansion into HLSL vector and matrix templates.

template <typename T, typename... Rest>
vector<T, 1 + sizeof...(Rest)> MakeVector(T First, Rest... Others) {
  return vector<T, 1 + sizeof...(Rest)>(First, Others...);
}

template <typename T, int... Dims>
struct MatrixWrapper {
  matrix<T, Dims...> M;
};

// CHECK: OpName %MatrixWrapper "MatrixWrapper"
// CHECK: OpMemberName %MatrixWrapper 0 "M"
// CHECK: OpName %MakeVector "MakeVector"

// CHECK-LABEL: %src_main = OpFunction %v4float
float4 main(float4 a : A) : SV_Target {
  // CHECK: OpFunctionCall %v4float %MakeVector
  vector<float, 4> v = MakeVector(a.x, a.y, a.z, a.w);

  MatrixWrapper<float, 2, 2> mw;
  // CHECK: OpCompositeConstruct %mat2v2float
  mw.M = matrix<float, 2, 2>(v.x, v.y, v.z, v.w);

  return float4(mw.M._11, mw.M._22, v.z, v.w);
}

// CHECK-LABEL: %MakeVector = OpFunction %v4float
