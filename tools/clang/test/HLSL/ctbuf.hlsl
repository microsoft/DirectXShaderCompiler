// RUN: %dxc -T lib_6_3 -ast-dump %s | FileCheck %s

struct S {
  float4 f;
};

// CHECK: ConstantBuffer
// CHECK: <line:9:16, col:19> col:19 myCBuffer 'const S'
ConstantBuffer<S> myCBuffer;
// CHECK: TextureBuffer
// CHECK: <line:12:15, col:18> col:18 myTBffer 'const S'
TextureBuffer<S> myTBffer;

// CHECK: cbuffer
// CHECK: <line:18:3, col:5> col:5 c0 'const S'

cbuffer A {
  S c0;
};

// CHECK: tbuffer
// CHECK: <line:24:3, col:5> col:5 t0 'const S'
tbuffer B {
  S t0;
};
float main() : A {
  return 1.0;
}
