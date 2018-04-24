// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

// Verify error on on incomplete array in a struct or class

typedef const int inta[];

static inta s_test1 = {1, 2, 3};

static int s_test2[] = { 4, 5, 6 };

struct foo1 {
  float4 member;
  inta a;   // expected-error {{array dimensions of struct/class members must be explicit}}
};

struct foo2 {
  int a[];  // expected-error {{array dimensions of struct/class members must be explicit}}
  float4 member;
};

class foo3 {
  float4 member;
  inta a;   // expected-error {{array dimensions of struct/class members must be explicit}}
};

class foo4 {
  float4 member;
  int a[];  // expected-error {{array dimensions of struct/class members must be explicit}}
};
