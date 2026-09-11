// RUN: %dxc -T lib_6_3 -HV 202x -verify %s
// RUN: %dxc -T lib_6_3 -HV 202x -ast-dump %s 2>&1 | FileCheck %s

// Verify HLSL 202x variadic templates in semantic analysis and the AST.

// expected-no-diagnostics

// Template type parameter pack.
// CHECK: FunctionTemplateDecl {{.*}} Sum
// CHECK: TemplateTypeParmDecl {{.*}} typename ... Rest
template <typename T, typename... Rest>
T Sum(T First, Rest... Others) {
  return First;
}

template <typename T>
T Sum(T First) {
  return First;
}

// Non-type template parameter pack.
// CHECK: ClassTemplateDecl {{.*}} IntPack
// CHECK: NonTypeTemplateParmDecl {{.*}} 'int' ... Values
template <int... Values>
struct IntPack {
  static const int Count = sizeof...(Values);
};

// A pack of types forwarded as a template argument list to another
// variadic template.
template <typename... Args>
struct Tuple {
  static const uint Size = sizeof...(Args);
};

template <typename... Args>
uint CountArgs(Args... args) {
  return sizeof...(Args);
}

// Pack expansion forwarding a parameter pack as a template argument list.
template <typename... Args>
uint Forward(Args... args) {
  return Tuple<Args...>::Size;
}

// Recursive class-template partial specialization peeling one type off a
// pack at a time is the canonical variadic-template pattern for
// compile-time recursion and depends on partial ordering between the
// primary template and the partial specialization -- Sema machinery
// that HLSL previously never exercised at all.
// CHECK: ClassTemplatePartialSpecializationDecl {{.*}} PackLength
// CHECK: TemplateTypeParmDecl {{.*}} typename ... Rest
template <typename... Ts>
struct PackLength {
  static const uint Value = 0;
};
template <typename T, typename... Rest>
struct PackLength<T, Rest...> {
  static const uint Value = 1 + PackLength<Rest...>::Value;
};
// The fully-empty-pack explicit specialization is also exercised, since it
// is the recursion's base case.
template <>
struct PackLength<> {
  static const uint Value = 0;
};

template <typename T, typename... Rest>
vector<T, 1 + sizeof...(Rest)> MakeVector(T First, Rest... Others) {
  return vector<T, 1 + sizeof...(Rest)>(First, Others...);
}

// Pack expansion inside a braced-init-list (a construct HLSL parses via
// its own, more restrictive initializer-list parsing, distinct from the
// call-argument and template-argument-list pack-expansion contexts).
template <typename T, typename... Rest>
T SumArray(T First, Rest... Others) {
  T values[1 + sizeof...(Rest)] = {First, Others...};
  T total = (T)0;
  for (int i = 0; i < 1 + sizeof...(Rest); ++i)
    total += values[i];
  return total;
}

// CHECK: ClassTemplateDecl {{.*}} BufferHolder
// CHECK: TemplateTypeParmDecl {{.*}} typename ... Ts
template <typename... Ts>
struct BufferHolder {
  StructuredBuffer<Ts...> Buf;
};
BufferHolder<float> g_FloatHolder;
BufferHolder<int> g_IntHolder;

// CHECK: ClassTemplateDecl {{.*}} MatrixWrapper
// CHECK: NonTypeTemplateParmDecl {{.*}} 'int' ... Dims
template <typename T, int... Dims>
struct MatrixWrapper {
  matrix<T, Dims...> M;
};

// A member (nested) template with its own, independent parameter pack,
// combined with the pack of its enclosing class template.
template <typename... Ts>
struct Zipper {
  template <typename... Us>
  static uint Count(Ts... ts, Us... us) {
    return sizeof...(Ts) + sizeof...(Us);
  }
};

// Zero-argument (empty pack) instantiation.
uint TestEmptyPack() { return CountArgs(); }

export
float TestVariadic() {
  float a = Sum(1.0, 2.0, 3.0);
  uint b = CountArgs(1, 2, 3, 4);
  uint c = Forward(1, 2);
  const int d = IntPack<1, 2, 3>::Count;
  const uint e = PackLength<int, float, bool, uint>::Value;
  const uint eEmpty = PackLength<>::Value;
  vector<float, 4> v = MakeVector(1.0, 2.0, 3.0, 4.0);
  float f = SumArray(1.0, 2.0, 3.0, 4.0, 5.0);
  MatrixWrapper<float, 2, 2> mw;
  mw.M = matrix<float, 2, 2>(1, 2, 3, 4);
  uint z = Zipper<int, float>::Count<double>(1, 2.0, 3.0);
  uint empty = TestEmptyPack();
  return a + b + c + d + e + eEmpty + v.x + f + mw.M._11 + z + empty +
         (float)g_FloatHolder.Buf.Load(0) + (float)g_IntHolder.Buf.Load(0);
}
