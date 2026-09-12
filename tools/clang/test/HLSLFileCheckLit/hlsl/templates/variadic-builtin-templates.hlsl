// RUN: %dxc -E main -T ps_6_0 -HV 202x %s | FileCheck %s

// Verify pack expansion into HLSL vector, matrix, and resource templates.

template <typename... Ts>
struct Holder {
  StructuredBuffer<Ts...> Buf;
};
Holder<float> g_Holder : register(t0);

template <typename T, typename... Rest>
vector<T, 1 + sizeof...(Rest)> MakeVector(T First, Rest... Others) {
  return vector<T, 1 + sizeof...(Rest)>(First, Others...);
}

template <typename T, int... Dims>
struct MatrixWrapper {
  matrix<T, Dims...> M;
};

// CHECK: call %dx.types.Handle @dx.op.createHandle(i32 57
// CHECK: call %dx.types.ResRet.f32 @dx.op.bufferLoad.f32
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3
float4 main(float4 a : A) : SV_Target {
  vector<float, 4> v = MakeVector(a.x, a.y, a.z, a.w);

  MatrixWrapper<float, 2, 2> mw;
  mw.M = matrix<float, 2, 2>(v.x, v.y, v.z, v.w);

  float bufVal = g_Holder.Buf.Load(0);

  return float4(mw.M._11, mw.M._22, bufVal, v.w);
}
