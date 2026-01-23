// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

export void fn1(groupshared uint Sh) {
// expected-error@-1{{groupshared and export/noinline cannot be used together for a parameter}}
  Sh = 5;
}

[noinline] void fn2(groupshared uint Sh) {
// expected-error@-1{{groupshared and export/noinline cannot be used together for a parameter}}
  Sh = 6;
}
