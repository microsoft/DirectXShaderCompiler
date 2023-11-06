// RUN: %clang_cc1 -enable-unions -fsyntax-only -ffreestanding -verify %s
// RUN: %clang_cc1 -HV 2021 -fsyntax-only -ffreestanding -verify %s

// Tests declarations and uses of anonymous unions.

void main() {
  union { /* expected-error {{anonymous unions are not supported in HLSL}} */
    uint a;
  };
  a = 1;
}
