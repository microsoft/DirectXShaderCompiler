// RUN: %dxc -E main -T vs_6_6 -HV 2021 -enable-16bit-types -ast-dump %s | FileCheck %s

// CHECK: error: 'MyStruct' cannot be used as a type parameter where a scalar is required
// CHECK-NEXT: Texture2D<vector<T, 4> > tex = ResourceDescriptorHeap[0];

// CHECK: note: in instantiation of function template specialization 'foo<MyStruct>' requested here
// CHECK-NEXT: return foo<MyStruct>().f;

template<typename T>
T foo() {
  Texture2D<vector<T, 4> > tex = ResourceDescriptorHeap[0];
  return tex[uint2(0,0)].x;
}

struct MyStruct {
  float f;
};

float main() : OUT {
  return foo<MyStruct>().f;
}
