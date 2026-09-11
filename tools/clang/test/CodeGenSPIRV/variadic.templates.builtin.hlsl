// RUN: %dxc -E main -T ps_6_0 -HV 202x -fcgl %s -spirv | FileCheck %s

// Verify pack expansion into HLSL vector, matrix, and resource templates.

template <typename... Ts>
struct Holder {
  StructuredBuffer<Ts...> Buf;
};

template <typename T, typename... Rest>
vector<T, 1 + sizeof...(Rest)> MakeVector(T First, Rest... Others) {
  return vector<T, 1 + sizeof...(Rest)>(First, Others...);
}

template <typename T, int... Dims>
struct MatrixWrapper {
  matrix<T, Dims...> M;
};

// CHECK: OpName %type_StructuredBuffer_float "type.StructuredBuffer.float"
// CHECK: OpName %g_Holder "g_Holder"
// CHECK: OpName %MatrixWrapper "MatrixWrapper"
// CHECK: OpMemberName %MatrixWrapper 0 "M"
// CHECK: OpName %MakeVector "MakeVector"
Holder<float> g_Holder : register(t0);

// CHECK-LABEL: %src_main = OpFunction %v4float
float4 main(float4 a : A) : SV_Target {
  // CHECK: OpFunctionCall %v4float %MakeVector
  vector<float, 4> v = MakeVector(a.x, a.y, a.z, a.w);

  MatrixWrapper<float, 2, 2> mw;
  // CHECK: OpCompositeConstruct %mat2v2float
  mw.M = matrix<float, 2, 2>(v.x, v.y, v.z, v.w);

  // CHECK: OpAccessChain %_ptr_Uniform_float {{%[0-9]+}} %int_0 %int_0
  float bufVal = g_Holder.Buf.Load(0);

  return float4(mw.M._11, mw.M._22, bufVal, v.w);
}

// CHECK-LABEL: %MakeVector = OpFunction %v4float
