// RUN: %dxc -T lib_6_3 -HV 2021 -verify %s

// In HLSL versions earlier than 202x, 'static_assert' remains available as an
// ordinary identifier.
// expected-no-diagnostics

int static_assert = 1;

int use_static_assert(int static_assert) {
  return static_assert;
}
