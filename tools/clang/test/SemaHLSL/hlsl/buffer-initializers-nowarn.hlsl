// RUN: %dxc -T lib_6_9 -HV 2021 -Wno-hlsl-buffer-initializer -verify %s

// The pre-202x warning can be suppressed with -Wno-hlsl-buffer-initializer.
// expected-no-diagnostics

float Implicit = 1;

cbuffer CB {
  float CBMember = 2;
}

tbuffer TB {
  float TBMember = 3;
}

float main() : OUT {
  return Implicit + CBMember + TBMember;
}
