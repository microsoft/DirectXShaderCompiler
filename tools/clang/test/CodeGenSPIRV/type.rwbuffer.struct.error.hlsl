// RUN: not %dxc -DBUF=RWBuffer                -T vs_6_0 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s -check-prefix=BUF
// RUN: not %dxc -DBUF=RasterizerOrderedBuffer -T vs_6_0 -E main -fcgl  %s -spirv  2>&1 | FileCheck %s -check-prefix=ROV

struct S {
  float a;
  float b;
};

// BUF: error: Elements of typed buffers and textures must be scalars or vectors
// ROV: error: cannot instantiate RasterizerOrderedBuffer with struct type 'S'
BUF<S> Buf;

float4 main() : A {
  return 1.0;
}

