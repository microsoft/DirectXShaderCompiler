// RUN: %dxc -E main -T ps_6_0 -HV 202x %s | FileCheck %s

// Compare pack-expanded and explicit initializer-list scalarization using
// shader inputs so the results remain observable in DXIL.

struct PairOf2 {
  float2 lo;
  float2 hi;
};

// Vector target filled from mixed vector+vector pack elements.
template <typename... Ts>
float4 PackVectorMixed(Ts... vals) {
  float4 v = { vals... };
  return v;
}
float4 ManualVectorMixed(float2 a, float2 b) {
  float4 v = { a, b };
  return v;
}

// A flat initializer list overflowing across the boundary of an array of
// vectors.
template <typename... Ts>
float2 PackArrayOverflow(Ts... vals) {
  float2 arr[2] = { vals... };
  return arr[0] + arr[1];
}
float2 ManualArrayOverflow(float a, float b, float c, float d) {
  float2 arr[2] = { a, b, c, d };
  return arr[0] + arr[1];
}

// Struct-member scalarization, including a vector-typed member.
template <typename... Ts>
PairOf2 PackStruct(Ts... vals) {
  PairOf2 s = { vals... };
  return s;
}
PairOf2 ManualStruct(float2 a, float2 b) {
  PairOf2 s = { a, b };
  return s;
}

// CHECK: define void @main()
// CHECK-DAG: fmul fast float %{{.*}}, 2.000000e+00
// CHECK-DAG: fmul fast float %{{.*}}, 6.000000e+00
// CHECK-DAG: fmul fast float %{{.*}}, 4.000000e+00
// CHECK-DAG: fmul fast float %{{.*}}, 4.000000e+00
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2
// CHECK: call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3
float4 main(float4 inp : A) : SV_Target {
  float2 lo = inp.xy;
  float2 hi = inp.zw;

  float4 vp = PackVectorMixed(lo, hi);
  float4 vm = ManualVectorMixed(lo, hi);

  float2 op = PackArrayOverflow(inp.x, inp.y, inp.z, inp.w);
  float2 om = ManualArrayOverflow(inp.x, inp.y, inp.z, inp.w);

  PairOf2 sp = PackStruct(lo, hi);
  PairOf2 sm = ManualStruct(lo, hi);

  return vp + vm + float4(op + om, 0, 0) +
         float4(sp.lo + sm.lo, sp.hi + sm.hi);
}
