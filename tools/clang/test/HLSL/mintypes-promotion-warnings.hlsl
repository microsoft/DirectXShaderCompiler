// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -verify -verify-ignore-unexpected=note %s

// Verify minfloat/minint promotion warnings for shader input/outputs

void main(
  min10float in_f10, // expected-warning {{min10float is promoted to min16float}}
  min12int in_i12, // expected-warning {{min12int is promoted to min16int}} 
  out min10float out_f10, // expected-warning {{min10float is promoted to min16float}}
  out min12int out_i12) {} // expected-warning {{min12int is promoted to min16int}} 