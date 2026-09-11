// RUN: %dxc -T lib_6_3 -HV 202x -verify %s
// RUN: %dxc -T lib_6_3 -HV 202x -ast-dump %s 2>&1 | FileCheck %s

// Compare pack-expanded and explicit initializer-list scalarization.

// expected-no-diagnostics

struct ScalarizeStruct {
  float a;
  float2 b;
  float c;
};

// Exact-count vector scalarization.

// CHECK: FunctionDecl {{.*}} used ManualVectorExact 'float4 (float, float, float, float)'
// CHECK: InitListExpr {{.*}} 'float4':'vector<float, 4>'
float4 ManualVectorExact(float a, float b, float c, float d) {
  float4 v = { a, b, c, d };
  return v;
}

// CHECK: FunctionDecl {{.*}} used PackVectorExact 'float4 (float, float, float, float)'
// CHECK: InitListExpr {{.*}} 'float4':'vector<float, 4>'
template <typename... Ts>
float4 PackVectorExact(Ts... vals) {
  float4 v = { vals... };
  return v;
}

// Mixed scalar and vector elements.

// CHECK: FunctionDecl {{.*}} used ManualVectorMixed 'float3 (float2, float)'
// CHECK: InitListExpr {{.*}} 'float3':'vector<float, 3>'
float3 ManualVectorMixed(float2 a, float b) {
  float3 v = { a, b };
  return v;
}

// CHECK: FunctionDecl {{.*}} used PackVectorMixed 'float3 (vector<float, 2>, float)'
// CHECK: InitListExpr {{.*}} 'float3':'vector<float, 3>'
template <typename... Ts>
float3 PackVectorMixed(Ts... vals) {
  float3 v = { vals... };
  return v;
}

// Scalar overflow across array elements.

float2 ManualArrayOverflow(float a, float b, float c, float d) {
  float2 arr[2] = { a, b, c, d };
  return arr[0] + arr[1];
}

template <typename... Ts>
float2 PackArrayOverflow(Ts... vals) {
  float2 arr[2] = { vals... };
  return arr[0] + arr[1];
}

// Struct-member scalarization.

ScalarizeStruct ManualStruct(float a, float2 b, float c) {
  ScalarizeStruct s = { a, b, c };
  return s;
}

template <typename... Ts>
ScalarizeStruct PackStruct(Ts... vals) {
  ScalarizeStruct s = { vals... };
  return s;
}

// Matrix scalarization.

float2x2 ManualMatrix(float a, float b, float c, float d) {
  float2x2 m = { a, b, c, d };
  return m;
}

template <typename... Ts>
float2x2 PackMatrix(Ts... vals) {
  float2x2 m = { vals... };
  return m;
}

export
float TestInitListScalarization() {
  float4 v1 = ManualVectorExact(1.0, 2.0, 3.0, 4.0);
  float4 v2 = PackVectorExact(1.0, 2.0, 3.0, 4.0);

  float2 b2 = float2(1.0, 2.0);
  float3 v3 = ManualVectorMixed(b2, 3.0);
  float3 v4 = PackVectorMixed(b2, 3.0);

  float o1 = ManualArrayOverflow(1.0, 2.0, 3.0, 4.0).x;
  float o2 = PackArrayOverflow(1.0, 2.0, 3.0, 4.0).x;

  ScalarizeStruct s1 = ManualStruct(1.0, b2, 2.0);
  ScalarizeStruct s2 = PackStruct(1.0, b2, 2.0);

  float2x2 m1 = ManualMatrix(1.0, 2.0, 3.0, 4.0);
  float2x2 m2 = PackMatrix(1.0, 2.0, 3.0, 4.0);

  return v1.x + v2.x + v3.x + v4.x + o1 + o2 + s1.a + s2.a + m1._11 + m2._11;
}
