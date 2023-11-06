// RUN: %clang_cc1 -enable-unions -fsyntax-only -ffreestanding -verify %s
// RUN: %clang_cc1 -HV 2021 -fsyntax-only -ffreestanding -verify %s
// Tests usage of the sizeof operator

union EmptyStruct {};
union SimpleStruct { int x; };
union StructWithResource { Buffer buf; int x; };

void main()
{
  // Type vs expression argument
  sizeof(int); // expected-warning {{expression result unused}}
  sizeof((int)0); // expected-warning {{expression result unused}}

  // Type shapes
  sizeof(int);    // expected-warning {{expression result unused}}
  sizeof(int2);   // expected-warning {{expression result unused}}
  sizeof(int2x2); // expected-warning {{expression result unused}}
  sizeof(int[2]); // expected-warning {{expression result unused}}
  sizeof(SimpleStruct); // expected-warning {{expression result unused}}
  sizeof(EmptyStruct); // expected-warning {{expression result unused}}

  // Special types
  sizeof(void);               // expected-error {{invalid application of 'sizeof' to an incomplete type 'void'}}
  sizeof 42;                  // expected-error {{invalid application of 'sizeof' to literal type 'literal int'}}
  sizeof 42.0;                // expected-error {{invalid application of 'sizeof' to literal type 'literal float'}}
  sizeof "";                  // expected-error {{invalid application of 'sizeof' to non-numeric type 'literal string'}}
  sizeof(Buffer);             // expected-error {{invalid application of 'sizeof' to non-numeric type 'Buffer'}}
  sizeof(StructWithResource); // expected-error {{invalid application of 'sizeof' to non-numeric type 'StructWithResource'}}
  sizeof(main);               // expected-error {{invalid application of 'sizeof' to non-numeric type 'void ()'}}
}
