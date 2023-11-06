// RUN: %clang_cc1 -enable-unions -fsyntax-only -ffreestanding -verify %s
// RUN: %clang_cc1 -HV 2021 -fsyntax-only -ffreestanding -verify %s
// expected-no-diagnostics

union C {
  static uint f1;
  uint f2;
};
[numthreads(1,1,1)]
void main() {
  C c;
  c.f2 = 1;
  return;
}
