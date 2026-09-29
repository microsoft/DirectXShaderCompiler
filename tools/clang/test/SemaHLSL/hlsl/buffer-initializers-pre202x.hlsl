// RUN: %dxc -T lib_6_9 -HV 2021 -verify %s

// Before HLSL 202x, initializers on shader constants are ignored with a
// warning. See buffer-initializers-202x.hlsl for the HLSL 202x behavior.

// Variables at global scope land in the implicit $Globals constant buffer.
float Implicit = 1;
// expected-warning@-1 {{initializer for a variable in a cbuffer will be ignored}}

const float ImplicitConst = 2;
// expected-warning@-1 {{initializer for a variable in a cbuffer will be ignored}}

float2 ImplicitVector = float2(1, 2);
// expected-warning@-1 {{initializer for a variable in a cbuffer will be ignored}}

float ImplicitArray[2] = {1, 2};
// expected-warning@-1 {{initializer for a variable in a cbuffer will be ignored}}

struct S { float F; };
S ImplicitStruct = {1};
// expected-warning@-1 {{initializer for a variable in a cbuffer will be ignored}}

namespace N {
float NamespacedImplicit = 3;
// expected-warning@-1 {{initializer for a variable in a cbuffer will be ignored}}
}

cbuffer CB {
  float CBMember = 4;
  // expected-warning@-1 {{initializer for a variable in a cbuffer will be ignored}}

  // 'static' members are not part of the buffer, so they keep their
  // initializer.
  static float CBStatic = 5;
}

tbuffer TB {
  float TBMember = 6;
  // expected-warning@-1 {{initializer for a variable in a tbuffer will be ignored}}

  static float TBStatic = 7;
}

// These are not shader constants, so initializers remain valid.
static float StaticGlobal = 8;
groupshared float GroupShared = 9;

float main() : OUT {
  float Local = 10;
  return Local + Implicit + CBMember + TBMember;
}
