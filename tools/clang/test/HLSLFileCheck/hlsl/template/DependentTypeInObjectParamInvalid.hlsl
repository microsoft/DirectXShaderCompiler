// RUN: %dxc -E main -T vs_6_0 -HV 2021 -ast-dump %s | FileCheck %s

// CHECK: error: 'MyStruct' cannot be used as a type parameter where a scalar is required
// CHECK-NEXT: T foo(Texture2D<vector<T, 4> > tex) {

// CHECK: note: in instantiation of template class 'Foo<MyStruct>' requested here
// CHECK-NEXT: Foo<MyStruct> foo;

template<typename T>
struct Foo {
  T foo(Texture2D<vector<T, 4> > tex) {
    return tex[uint2(0,0)].x;
  }
};

struct MyStruct {
  float f;
};

void main() {
  Foo<MyStruct> foo;
}
