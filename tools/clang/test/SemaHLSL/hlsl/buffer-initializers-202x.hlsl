// RUN: %dxc -T lib_6_9 -HV 202x -verify %s

// Starting with HLSL 202x, initializers on shader constants are an error.
// See buffer-initializers-pre202x.hlsl for earlier language versions.

// Variables at global scope land in the implicit $Globals constant buffer.
float Implicit = 1;
// expected-error@-1 {{variable in a cbuffer cannot have an initializer}}

const float ImplicitConst = 2;
// expected-error@-1 {{variable in a cbuffer cannot have an initializer}}

float2 ImplicitVector = float2(1, 2);
// expected-error@-1 {{variable in a cbuffer cannot have an initializer}}

float ImplicitArray[2] = {1, 2};
// expected-error@-1 {{variable in a cbuffer cannot have an initializer}}

struct S { float F; };
S ImplicitStruct = {1};
// expected-error@-1 {{variable in a cbuffer cannot have an initializer}}

namespace N {
float NamespacedImplicit = 3;
// expected-error@-1 {{variable in a cbuffer cannot have an initializer}}
}

cbuffer CB {
  float CBMember = 4;
  // expected-error@-1 {{variable in a cbuffer cannot have an initializer}}

  // 'static' members are not part of the buffer, so they keep their
  // initializer.
  static float CBStatic = 5;
}

tbuffer TB {
  float TBMember = 6;
  // expected-error@-1 {{variable in a tbuffer cannot have an initializer}}

  static float TBStatic = 7;
}

// Static globals are initialized normally. Groupshared initializers are
// accepted but ignored.
static float StaticGlobal = 8;
groupshared float GroupShared = 9;
// expected-warning@-1 {{initializer of 'groupshared' variable will be ignored}}

float main() : OUT {
  float Local = 10;
  return Local + Implicit + CBMember + TBMember;
}
