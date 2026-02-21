// RUN: %dxc -Tlib_6_3 -verify -HV 2021 %s

// This test verifies that dxcompiler generates an error when defining
// a conversion operator (cast operator), which is not supported in HLSL.

struct MyStruct {
  float4 f;

  // expected-error@+1 {{conversion operator is not supported in HLSL}}
  operator float4() {
    return 42;
  }
};

struct AnotherStruct {
  int x;

  // expected-error@+1 {{conversion operator is not supported in HLSL}}
  operator int() {
    return x;
  }

  // expected-error@+1 {{conversion operator is not supported in HLSL}}
  operator bool() {
    return x != 0;
  }
};
