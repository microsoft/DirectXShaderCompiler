// RUN: %dxc -T lib_6_3 -enable-16bit-types -HV 202x -verify %s

groupshared uint16_t SharedData;

void fn1(groupshared half Sh) {
// expected-note@-1{{candidate function not viable: no known conversion from '__attribute__((address_space(3))) uint16_t' to '__attribute__((address_space(3))) half &' for 1st argument}}
  Sh = 5;
}

void fn2() {
  fn1(SharedData);
  // expected-error@-1{{no matching function for call to 'fn1'}}
}
