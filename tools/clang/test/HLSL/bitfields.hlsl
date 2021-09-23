// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

typedef int T : 1; /* expected-error {{expected unqualified-id}} expected-error {{expected ';' after top level declarator}} */
static int sb : 1; /* expected-error {{expected unqualified-id}} expected-error {{expected ';' after top level declarator}} */

struct R {
  static uint bf : 8; /* expected-error {{static member 'bf' cannot be a bit-field}} */
  uint pad : 0; /* expected-error {{named bit-field 'pad' has zero width}} */
};

struct S {
  uint b : 33; /* expected-error {{size of bit-field 'b' (33 bits) exceeds size of its type (32 bits)}} */
  bool c : 39; /* expected-error {{size of bit-field 'c' (39 bits) exceeds size of its type (32 bits)}} */
  bool d : 3;
};

int a[sizeof(S) == 1 ? 1 : -1];

enum E {};

struct Z {};
typedef int Integer;

struct X {
  enum E : 1;
  Z : 1; /* expected-error {{anonymous bit-field has non-integral type 'Z'}} */
};

struct A {
  uint bitX : 4;
  uint bitY : 4;
  uint var;
};

void main(A a : IN) {
  int x;
  x = sizeof(a.bitX); /* expected-error {{invalid application of 'sizeof' to bit-field}} */
  x = sizeof((uint) a.bitX);
  x = sizeof(a.var ? a.bitX : a.bitY); // clang: error: invalid application of 'sizeof' to bit-field
  x = sizeof(a.var ? a.bitX : a.bitX); // clang: error: invalid application of 'sizeof' to bit-field
  x = sizeof(a.bitX = 3); /* expected-warning {{expression with side effects has no effect in an unevaluated context}} */ // clang: error: invalid application of 'sizeof' to bit-field
  x = sizeof(a.bitY += 3); /* expected-error {{invalid application of 'sizeof' to bit-field}} */
}
