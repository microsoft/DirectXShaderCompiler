// RUN: %dxc -T lib_6_3 -HV 202x -verify %s
// expected-no-diagnostics

static_assert(static_cast<int>(3.75f) == 3);
static_assert(static_cast<float>(2) == 2.0f);
static_assert(static_cast<bool>(0) == false);
static_assert(static_cast<uint>(-1) == 0xffffffffu);

template <typename T, typename U>
T convert(U value) {
  return static_cast<T>(value);
}

struct S {
  float value;
};

struct Derived : S {
  float extra;
};

void copy_values(const S s, Derived d) {
  S copy = static_cast<S>(s);
  S base = static_cast<S>(d);
}

void conversions(float f, float4 v, float2x2 m, S s) {
  int i = static_cast<int>(f);
  float x = static_cast<float>(i);
  bool b = static_cast<bool>(f);
  int4 iv = static_cast<int4>(v);
  float2 truncated = static_cast<float2>(v);
  float4 splat = static_cast<float4>(f);
  int2x2 im = static_cast<int2x2>(m);
  S copy = static_cast<S>(s);
  int ti = convert<int>(f);
  int4 tv = convert<int4>(v);
  int4 nested = static_cast<vector<int, 4>>(v);
  static_cast<void>(f);
}
