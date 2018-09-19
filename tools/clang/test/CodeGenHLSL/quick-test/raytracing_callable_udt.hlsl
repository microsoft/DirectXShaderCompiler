// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// CHECK: error: callable parameter must be a user defined type with only numeric contents.

struct Foo {
  float4 vec;
  RWByteAddressBuffer buf;
};

struct MyParams {
  Foo foo;
};

[shader("callable")]
void callable_udt( inout MyParams param ) {}
