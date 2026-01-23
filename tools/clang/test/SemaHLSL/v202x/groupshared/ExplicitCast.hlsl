// RUN: %dxc -T lib_6_3 -enable-16bit-types -HV 202x -verify %s

groupshared uint16_t SharedData;

void fn1(groupshared half Sh) {
// expected-note@-1{{candidate function not viable: 1st argument ('half') is in address space 0, but parameter must be in address space 3}}
  Sh = 5;
}

void fn2() {
  fn1((half)SharedData);
  // expected-error@-1{{no matching function for call to 'fn1'}}
}
