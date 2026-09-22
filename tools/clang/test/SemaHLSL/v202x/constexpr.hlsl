// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// expected-no-diagnostics

#if !__has_feature(cxx_constexpr)
#error HLSL 202x should report constexpr as a feature
#endif

// Basic constexpr variable.
constexpr int g_constexpr = 3;

// constexpr functions.
constexpr int square(int x) { return x * x; }
constexpr int add(int a, int b) { return a + b; }

// Constexpr used as array bound.
static const int Arr[square(4)] = { 0, 0, 0, 0, 0, 0, 0, 0,
                                    0, 0, 0, 0, 0, 0, 0, 0 };

// Local constexpr in a function.
[shader("compute")]
[numthreads(1,1,1)]
void main() {
  constexpr int local = add(g_constexpr, square(2));
  int arr[local];
  (void)arr;
  (void)Arr[0];
}
