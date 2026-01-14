// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

groupshared uint SharedData;

void fn1(inout uint Sh) {
  Sh = 5;
}

void fn2() {
  fn1(SharedData);
// expected-warning@-1{{Passing groupshared variable to a parameter annotated with inout. See'groupshared' parameter annotation added in 202x}}
}
