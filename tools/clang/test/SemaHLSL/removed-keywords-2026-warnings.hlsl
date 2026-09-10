// RUN: %dxc -T lib_6_3 -HV 2021 -verify %s
// RUN: %dxc -T lib_6_3 -HV 2021 -DWARN=1 -Whlsl-2026-compat -verify %s

#if WARN

shared float globalShared; // expected-warning {{'shared' is ignored and will be removed in HLSL 2026}}
uniform float globalUniform; // expected-warning {{'uniform' is ignored and will be removed in HLSL 2026}}

float useUniformParameter(uniform float value) { // expected-warning {{'uniform' is ignored and will be removed in HLSL 2026}}
  return value;
}

#else

// expected-no-diagnostics
shared float globalShared;
uniform float globalUniform;

float useUniformParameter(uniform float value) {
  return value;
}

#endif
