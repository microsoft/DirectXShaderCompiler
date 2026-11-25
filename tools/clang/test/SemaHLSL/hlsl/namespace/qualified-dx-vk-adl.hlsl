// RUN: %dxc -T lib_6_3 -HV 2021 -verify %s
// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

namespace N {
namespace vk {
float f(float x) { return x; } // expected-note{{passing argument to parameter 'x' here}}
}
namespace dx {
float f(float x) { return x; } // expected-note{{passing argument to parameter 'x' here}}
}
}

float CallNestedVk(float x) { return N::vk::f(x); }
float CallNestedDx(float x) { return N::dx::f(x); }

namespace associated {
struct Value {
  float x;
};
float f(Value value) { return value.x; }
}

float NoADLForNestedVk(associated::Value value) {
  return N::vk::f(value); // expected-error{{cannot initialize a parameter of type 'float'}}
}

float NoADLForNestedDx(associated::Value value) {
  return N::dx::f(value); // expected-error{{cannot initialize a parameter of type 'float'}}
}
