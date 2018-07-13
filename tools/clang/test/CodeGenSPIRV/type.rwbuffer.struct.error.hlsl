// Run: %dxc -T vs_6_0 -E main

struct S {
  float a;
  float b;
};

// CHECK: error: cannot instantiate RWBuffer with struct type 'S'
RWBuffer<S> MyRWBuffer;

float4 main() : A {
  return 1.0;
}
