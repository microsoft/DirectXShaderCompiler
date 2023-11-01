// RUN: %clang_cc1 -enable-unions -HV 202x -Wno-unused-value -fsyntax-only -ffreestanding -verify -verify-ignore-unexpected=note %s

// Tests usage of the sizeof operator

union EmptyUnion {};
union SimpleUnion {
  int x;
};
union UnionWithResource {
  Buffer buf;
  int x;
};

void main() {
  // Type vs expression argument
  sizeof(int);
  sizeof((int)0);

  // Type shapes
  sizeof(int);
  sizeof(int2);
  sizeof(int2x2);
  sizeof(int[2]);
  sizeof(SimpleUnion);
  sizeof(EmptyUnion);

  // Special types
  sizeof(void);               // expected-error {{invalid application of 'sizeof' to an incomplete type 'void'}}
  sizeof 42;                  // expected-error {{invalid application of 'sizeof' to literal type 'literal int'}}
  sizeof 42.0;                // expected-error {{invalid application of 'sizeof' to literal type 'literal float'}}
  sizeof "";                  // expected-error {{invalid application of 'sizeof' to non-numeric type 'literal string'}}
  sizeof(Buffer);             // expected-error {{invalid application of 'sizeof' to non-numeric type 'Buffer'}}
  sizeof(UnionWithResource); // expected-error {{invalid application of 'sizeof' to non-numeric type 'UnionWithResource'}}
  sizeof(main);               // expected-error {{invalid application of 'sizeof' to non-numeric type 'void ()'}}
}
