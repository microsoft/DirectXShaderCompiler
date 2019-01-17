// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -verify %s

// Tests all implicit conversions and explicit casts between type shapes
// (scalars, vectors, matrices, arrays and structs).

// Explicit casts are assumed to be "stronger" than implicit conversions.
// > If an implicit conversion succeeds, we don't test the explicit cast
// > If an explicit cast fails, we don't test the implicit conversion

typedef int A1[1];
typedef int A2[2];
typedef int A4[4];
typedef int A5[5];
struct S1 { int a; };
struct S2 { int a, b; };
struct S4 { int a, b, c, d; };
struct S5 { int a, b, c, d, e; };

void to_i(int i) {}
void to_v1(int1 v) {}
void to_v2(int2 v) {}
void to_v4(int4 v) {}
void to_m1x1(int1x1 m) {}
void to_m1x2(int1x2 m) {}
void to_m2x1(int2x1 m) {}
void to_m2x2(int2x2 m) {}
void to_m3x3(int3x3 m) {}
void to_a1(A1 a) {}
void to_a2(A2 a) {}
void to_a4(A4 a) {}
void to_a5(A5 a) {}
void to_s1(S1 s) {}
void to_s2(S2 s) {}
void to_s4(S4 s) {}
void to_s5(S5 s) {}

void main()
{
    int i = 1;
    int1 v1 = int1(1);
    int2 v2 = int2(1, 2);
    int4 v4 = int4(1, 2, 3, 4);
    int1x1 m1x1 = int1x1(11);
    int1x2 m1x2 = int1x2(11, 12);
    int2x1 m2x1 = int2x1(11, 21);
    int2x2 m2x2 = int2x2(11, 12, 21, 22);
    int3x3 m3x3 = int3x3(11, 12, 13, 21, 22, 23, 31, 32, 33);
    A1 a1 = { 1 };
    A2 a2 = { 1, 2 };
    A4 a4 = { 1, 2, 3, 4 };
    S1 s1 = { 1 };
    S2 s2 = { 1, 2 };
    S4 s4 = { 1, 2, 3, 4 };

    // =========== Scalar/single-element ===========
    to_i(v1);
    to_i(m1x1);
    to_i(a1);                                               /* expected-error {{no matching function for call to 'to_i'}} fxc-error {{X3017: 'to_i': cannot convert from 'typedef int[1]' to 'int'}} */
    (int)a1;
    to_i(s1);                                               /* expected-error {{no matching function for call to 'to_i'}} fxc-error {{X3017: 'to_i': cannot convert from 'struct S1' to 'int'}} */
    (int)s1;

    to_v1(i);
    to_v1(m1x1);
    to_v1(a1);                                              /* expected-error {{no matching function for call to 'to_v1'}} fxc-error {{X3017: 'to_v1': cannot convert from 'typedef int[1]' to 'int1'}} */
    (int1)a1;
    to_v1(s1);                                              /* expected-error {{no matching function for call to 'to_v1'}} fxc-error {{X3017: 'to_v1': cannot convert from 'struct S1' to 'int1'}} */
    (int1)s1;

    to_m1x1(i);
    to_m1x1(v1);
    to_m1x1(a1);                                            /* expected-error {{no matching function for call to 'to_m1x1'}} fxc-error {{X3017: 'to_m1x1': cannot convert from 'typedef int[1]' to 'int1'}} */
    (int1x1)a1;
    to_m1x1(s1);                                            /* expected-error {{no matching function for call to 'to_m1x1'}} fxc-error {{X3017: 'to_m1x1': cannot convert from 'struct S1' to 'int1'}} */
    (int1x1)s1;

    to_a1(i);                                               /* expected-error {{no matching function for call to 'to_a1'}} fxc-error {{X3017: 'to_a1': cannot convert from 'int' to 'typedef int[1]'}} */
    (A1)i;
    to_a1(v1);                                              /* expected-error {{no matching function for call to 'to_a1'}} fxc-error {{X3017: 'to_a1': cannot convert from 'int1' to 'typedef int[1]'}} */
    (A1)v1;
    to_a1(m1x1);                                            /* expected-error {{no matching function for call to 'to_a1'}} fxc-error {{X3017: 'to_a1': cannot convert from 'int1' to 'typedef int[1]'}} */
    (A1)m1x1;
    to_a1(s1);                                              /* expected-error {{no matching function for call to 'to_a1'}} fxc-pass {{}} */
    (A1)s1;

    to_s1(i);                                               /* expected-error {{no matching function for call to 'to_s1'}} fxc-error {{X3017: 'to_s1': cannot convert from 'int' to 'struct S1'}} */
    (S1)i;
    to_s1(v1);                                              /* expected-error {{no matching function for call to 'to_s1'}} fxc-error {{X3017: 'to_s1': cannot convert from 'int1' to 'struct S1'}} */
    (S1)v1;
    to_s1(m1x1);                                            /* expected-error {{no matching function for call to 'to_s1'}} fxc-error {{X3017: 'to_s1': cannot convert from 'int1' to 'struct S1'}} */
    (S1)m1x1;
    to_s1(a1);                                              /* expected-error {{no matching function for call to 'to_s1'}} fxc-pass {{}} */
    (S1)a1;

    // =========== Truncation to scalar/single-element ===========
    // Single element sources already tested
    to_i(v2);                                               /* expected-warning {{implicit truncation of vector type}} fxc-warning {{X3206: 'to_i': implicit truncation of vector type}} */
    to_i(m2x2);                                             /* expected-warning {{implicit truncation of vector type}} fxc-warning {{X3206: 'to_i': implicit truncation of vector type}} */
    to_i(a2);                                               /* expected-error {{no matching function for call to 'to_i'}} fxc-error {{X3017: 'to_i': cannot convert from 'typedef int[2]' to 'int'}} */
    (int)a2;
    to_i(s2);                                               /* expected-error {{no matching function for call to 'to_i'}} fxc-error {{X3017: 'to_i': cannot convert from 'struct S2' to 'int'}} */
    (int)s2;

    to_v1(v2);                                              /* expected-warning {{implicit truncation of vector type}} fxc-warning {{X3206: 'to_v1': implicit truncation of vector type}} */
    to_v1(m2x2);                                            /* expected-warning {{implicit truncation of vector type}} fxc-warning {{X3206: 'to_v1': implicit truncation of vector type}} */
    to_v1(a2);                                              /* expected-error {{no matching function for call to 'to_v1'}} fxc-error {{X3017: 'to_v1': cannot convert from 'typedef int[2]' to 'int1'}} */
    (int1)a2;
    to_v1(s2);                                              /* expected-error {{no matching function for call to 'to_v1'}} fxc-error {{X3017: 'to_v1': cannot convert from 'struct S2' to 'int1'}} */
    (int1)s2;

    to_m1x1(v2);                                            /* expected-warning {{implicit truncation of vector type}} fxc-warning {{X3206: 'to_m1x1': implicit truncation of vector type}} */
    to_m1x1(m2x2);                                          /* expected-warning {{implicit truncation of vector type}} fxc-warning {{X3206: 'to_m1x1': implicit truncation of vector type}} */
    to_m1x1(a2);                                            /* expected-error {{no matching function for call to 'to_m1x1'}} fxc-error {{X3017: 'to_m1x1': cannot convert from 'typedef int[2]' to 'int1'}} */
    (int1x1)a2;
    to_m1x1(s2);                                            /* expected-error {{no matching function for call to 'to_m1x1'}} fxc-error {{X3017: 'to_m1x1': cannot convert from 'struct S2' to 'int1'}} */
    (int1x1)s2;

    to_a1(v2);                                              /* expected-error {{no matching function for call to 'to_a1'}} fxc-error {{X3017: 'to_a1': cannot convert from 'int2' to 'typedef int[1]'}} */
    to_a1(m2x2);                                            /* expected-error {{no matching function for call to 'to_a1'}} fxc-error {{X3017: 'to_a1': cannot implicitly convert from 'int2x2' to 'typedef int[1]'}} */
    to_a1(a2);                                              /* expected-error {{no matching function for call to 'to_a1'}} fxc-error {{X3017: 'to_a1': cannot convert from 'typedef int[2]' to 'typedef int[1]'}} */
    (A1)a2;
    to_a1(s2);                                              /* expected-error {{no matching function for call to 'to_a1'}} fxc-error {{X3017: 'to_a1': cannot convert from 'struct S2' to 'typedef int[1]'}} */
    (A1)s2;

    to_s1(v2);                                              /* expected-error {{no matching function for call to 'to_s1'}} fxc-error {{X3017: 'to_s1': cannot convert from 'int2' to 'struct S1'}} */
    to_s1(m2x2);                                            /* expected-error {{no matching function for call to 'to_s1'}} fxc-error {{X3017: 'to_s1': cannot implicitly convert from 'int2x2' to 'struct S1'}} */
    to_s1(a2);                                              /* expected-error {{no matching function for call to 'to_s1'}} fxc-error {{X3017: 'to_s1': cannot convert from 'typedef int[2]' to 'struct S1'}} */
    (S1)a2;
    to_s1(s2);                                              /* expected-error {{no matching function for call to 'to_s1'}} fxc-error {{X3017: 'to_s1': cannot convert from 'struct S2' to 'struct S1'}} */
    (S1)s2;

    // =========== Splatting ===========
    // Single element dests already tested
    to_v2(i);
    to_v2(v1);
    to_v2(m1x1);
    (int2)a1;                                               /* expected-error {{cannot convert from 'A1' (aka 'int [1]') to 'int2'}} fxc-error {{X3017: cannot convert from 'typedef int[1]' to 'int2'}} */
    (int2)s1;                                               /* expected-error {{cannot convert from 'S1' to 'int2'}} fxc-error {{X3017: cannot convert from 'struct S1' to 'int2'}} */

    to_m2x2(i);
    to_m2x2(v1);
    to_m2x2(m1x1);
    (int2x2)a1;                                             /* expected-error {{cannot convert from 'A1' (aka 'int [1]') to 'int2x2'}} fxc-error {{X3017: cannot convert from 'typedef int[1]' to 'int2x2'}} */
    (int2x2)s1;                                             /* expected-error {{cannot convert from 'S1' to 'int2x2'}} fxc-error {{X3017: cannot convert from 'struct S1' to 'int2x2'}} */

    to_a2(i);                                               /* expected-error {{no matching function for call to 'to_a2'}} fxc-error {{X3017: 'to_a2': cannot convert from 'int' to 'typedef int[2]'}} */
    (A2)i;
    to_a2(v1);                                              /* expected-error {{no matching function for call to 'to_a2'}} fxc-error {{X3017: 'to_a2': cannot convert from 'int1' to 'typedef int[2]'}} */
    (A2)v1;                                                 /* expected-error {{cannot convert from 'int1' to 'A2' (aka 'int [2]')}} fxc-pass {{}} */
    to_a2(m1x1);                                            /* expected-error {{no matching function for call to 'to_a2'}} fxc-error {{X3017: 'to_a2': cannot convert from 'int1' to 'typedef int[2]'}} */
    (A2)m1x1;                                               /* expected-error {{cannot convert from 'int1x1' to 'A2' (aka 'int [2]')}} fxc-pass {{}} */
    (A2)a1;                                                 /* expected-error {{cannot convert from 'A1' (aka 'int [1]') to 'A2' (aka 'int [2]')}} fxc-error {{X3017: cannot convert from 'typedef int[1]' to 'typedef int[2]'}} */
    (A2)s1;                                                 /* expected-error {{cannot convert from 'S1' to 'A2' (aka 'int [2]')}} fxc-error {{X3017: cannot convert from 'struct S1' to 'typedef int[2]'}} */

    to_s2(i);                                               /* expected-error {{no matching function for call to 'to_s2'}} fxc-error {{X3017: 'to_s2': cannot convert from 'int' to 'struct S2'}} */
    (S2)i;
    to_s2(v1);                                              /* expected-error {{no matching function for call to 'to_s2'}} fxc-error {{X3017: 'to_s2': cannot convert from 'int1' to 'struct S2'}} */
    (S2)v1;                                                 /* expected-error {{cannot convert from 'int1' to 'S2'}} fxc-pass {{}} */
    to_s2(m1x1);                                            /* expected-error {{no matching function for call to 'to_s2'}} fxc-error {{X3017: 'to_s2': cannot convert from 'int1' to 'struct S2'}} */
    (S2)m1x1;                                               /* expected-error {{cannot convert from 'int1x1' to 'S2'}} fxc-pass {{}} */
    (S2)a1;                                                 /* expected-error {{cannot convert from 'A1' (aka 'int [1]') to 'S2'}} fxc-error {{X3017: cannot convert from 'typedef int[1]' to 'struct S2'}} */
    (S2)s1;                                                 /* expected-error {{cannot convert from 'S1' to 'S2'}} fxc-error {{X3017: cannot convert from 'struct S1' to 'struct S2'}} */

    // =========== Element-preserving ===========
    // Single element sources/dests already tested
    to_v2(m1x2);
    to_v2(m2x1);
    to_v4(m2x2);
    to_v2(a2);                                              /* expected-error {{no matching function for call to 'to_v2'}} fxc-error {{X3017: 'to_v2': cannot convert from 'typedef int[2]' to 'int2'}} */
    (int2)a2;
    to_v2(s2);                                              /* expected-error {{no matching function for call to 'to_v2'}} fxc-error {{X3017: 'to_v2': cannot convert from 'struct S2' to 'int2'}} */
    (int2)s2;

    to_m1x2(v2);
    to_m2x1(v2);
    to_m2x2(v4);
    (int1x2)m2x1;                                           /* expected-error {{cannot convert from 'int2x1' to 'int1x2'}} fxc-error {{X3017: cannot convert from 'int2x1' to 'int2'}} */
    (int2x1)m1x2;                                           /* expected-error {{cannot convert from 'int1x2' to 'int2x1'}} fxc-error {{X3017: cannot convert from 'int2' to 'int2x1'}} */
    to_m1x2(a2);                                            /* expected-error {{no matching function for call to 'to_m1x2'}} fxc-error {{X3017: 'to_m1x2': cannot convert from 'typedef int[2]' to 'int2'}} */
    (int1x2)a2;
    to_m2x1(a2);                                            /* expected-error {{no matching function for call to 'to_m2x1'}} fxc-error {{X3017: 'to_m2x1': cannot convert from 'typedef int[2]' to 'int2x1'}} */
    (int2x1)a2;
    to_m2x2(a4);                                            /* expected-error {{no matching function for call to 'to_m2x2'}} fxc-error {{X3017: 'to_m2x2': cannot convert from 'typedef int[4]' to 'int2x2'}} */
    (int2x2)a4;
    to_m1x2(s2);                                            /* expected-error {{no matching function for call to 'to_m1x2'}} fxc-error {{X3017: 'to_m1x2': cannot convert from 'struct S2' to 'int2'}} */
    (int1x2)s2;
    to_m2x1(s2);                                            /* expected-error {{no matching function for call to 'to_m2x1'}} fxc-error {{X3017: 'to_m2x1': cannot convert from 'struct S2' to 'int2x1'}} */
    (int2x1)s2;
    to_m2x2(s4);                                            /* expected-error {{no matching function for call to 'to_m2x2'}} fxc-error {{X3017: 'to_m2x2': cannot convert from 'struct S4' to 'int2x2'}} */
    (int2x2)s4;

    to_a2(v2);                                              /* expected-error {{no matching function for call to 'to_a2'}} fxc-error {{X3017: 'to_a2': cannot convert from 'int2' to 'typedef int[2]'}} */
    (A2)v2;
    to_a2(m1x2);                                            /* expected-error {{no matching function for call to 'to_a2'}} fxc-error {{X3017: 'to_a2': cannot convert from 'int2' to 'typedef int[2]'}} */
    (A2)m1x2;
    to_a2(m2x1);                                            /* expected-error {{no matching function for call to 'to_a2'}} fxc-error {{X3017: 'to_a2': cannot convert from 'int2x1' to 'typedef int[2]'}} */
    (A2)m2x1;
    to_a4(m2x2);                                            /* expected-error {{no matching function for call to 'to_a4'}} fxc-error {{X3017: 'to_a4': cannot convert from 'int2x2' to 'typedef int[4]'}} */
    (A4)m2x2;
    to_a2(s2);                                              /* expected-error {{no matching function for call to 'to_a2'}} fxc-pass {{}} */
    (A2)s2;

    to_s2(v2);                                              /* expected-error {{no matching function for call to 'to_s2'}} fxc-error {{X3017: 'to_s2': cannot convert from 'int2' to 'struct S2'}} */
    (S2)v2;
    to_s2(m1x2);                                            /* expected-error {{no matching function for call to 'to_s2'}} fxc-error {{X3017: 'to_s2': cannot convert from 'int2' to 'struct S2'}} */
    (S2)m1x2;
    to_s2(m2x1);                                            /* expected-error {{no matching function for call to 'to_s2'}} fxc-error {{X3017: 'to_s2': cannot convert from 'int2x1' to 'struct S2'}} */
    (S2)m2x1;
    to_s4(m2x2);                                            /* expected-error {{no matching function for call to 'to_s4'}} fxc-error {{X3017: 'to_s4': cannot convert from 'int2x2' to 'struct S4'}} */
    (S4)m2x2;
    to_s2(a2);                                              /* expected-error {{no matching function for call to 'to_s2'}} fxc-pass {{}} */
    (S2)a2;

    // =========== Extending ===========
    // Single element sources already tested (splatting)
    (int4)v2;                                               /* expected-error {{cannot convert from 'int2' to 'int4'}} fxc-error {{X3017: cannot convert from 'int2' to 'int4'}} */
    (int4)m1x2;                                             /* expected-error {{cannot convert from 'int1x2' to 'int4'}} fxc-error {{X3017: cannot convert from 'int2' to 'int4'}} */
    (int4)m2x1;                                             /* expected-error {{cannot convert from 'int2x1' to 'int4'}} fxc-error {{X3017: cannot convert from 'int2x1' to 'int4'}} */
    (int4)a2;                                               /* expected-error {{cannot convert from 'A2' (aka 'int [2]') to 'int4'}} fxc-error {{X3017: cannot convert from 'typedef int[2]' to 'int4'}} */
    (int4)s2;                                               /* expected-error {{cannot convert from 'S2' to 'int4'}} fxc-error {{X3017: cannot convert from 'struct S2' to 'int4'}} */

    (int2x2)v2;                                             /* expected-error {{cannot convert from 'int2' to 'int2x2'}} fxc-error {{X3017: cannot convert from 'int2' to 'int2x2'}} */
    (int2x2)m1x2;                                           /* expected-error {{cannot convert from 'int1x2' to 'int2x2'}} fxc-error {{X3017: cannot convert from 'int2' to 'int2x2'}} */
    (int2x2)m2x1;                                           /* expected-error {{cannot convert from 'int2x1' to 'int2x2'}} fxc-error {{X3017: cannot convert from 'int2x1' to 'int2x2'}} */
    (int3x3)m2x2;                                           /* expected-error {{cannot convert from 'int2x2' to 'int3x3'}} fxc-error {{X3017: cannot convert from 'int2x2' to 'int3x3'}} */
    (int2x2)a2;                                             /* expected-error {{cannot convert from 'A2' (aka 'int [2]') to 'int2x2'}} fxc-error {{X3017: cannot convert from 'typedef int[2]' to 'int2x2'}} */
    (int2x2)s2;                                             /* expected-error {{cannot convert from 'S2' to 'int2x2'}} fxc-error {{X3017: cannot convert from 'struct S2' to 'int2x2'}} */

    (A4)v2;                                                 /* expected-error {{cannot convert from 'int2' to 'A4' (aka 'int [4]')}} fxc-error {{X3017: cannot convert from 'int2' to 'typedef int[4]'}} */
    (A4)m1x2;                                               /* expected-error {{cannot convert from 'int1x2' to 'A4' (aka 'int [4]')}} fxc-error {{X3017: cannot convert from 'int2' to 'typedef int[4]'}} */
    (A4)m2x1;                                               /* expected-error {{cannot convert from 'int2x1' to 'A4' (aka 'int [4]')}} fxc-error {{X3017: cannot convert from 'int2x1' to 'typedef int[4]'}} */
    (A5)m2x2;                                               /* expected-error {{cannot convert from 'int2x2' to 'A5' (aka 'int [5]')}} fxc-error {{X3017: cannot convert from 'int2x2' to 'typedef int[5]'}} */
    (A4)a2;                                                 /* expected-error {{cannot convert from 'A2' (aka 'int [2]') to 'A4' (aka 'int [4]')}} fxc-error {{X3017: cannot convert from 'typedef int[2]' to 'typedef int[4]'}} */
    (A4)s2;                                                 /* expected-error {{cannot convert from 'S2' to 'A4' (aka 'int [4]')}} fxc-error {{X3017: cannot convert from 'struct S2' to 'typedef int[4]'}} */

    (S4)v2;                                                 /* expected-error {{cannot convert from 'int2' to 'S4'}} fxc-error {{X3017: cannot convert from 'int2' to 'struct S4'}} */
    (S4)m1x2;                                               /* expected-error {{cannot convert from 'int1x2' to 'S4'}} fxc-error {{X3017: cannot convert from 'int2' to 'struct S4'}} */
    (S4)m2x1;                                               /* expected-error {{cannot convert from 'int2x1' to 'S4'}} fxc-error {{X3017: cannot convert from 'int2x1' to 'struct S4'}} */
    (S5)m2x2;                                               /* expected-error {{cannot convert from 'int2x2' to 'S5'}} fxc-error {{X3017: cannot convert from 'int2x2' to 'struct S5'}} */
    (S4)a2;                                                 /* expected-error {{cannot convert from 'A2' (aka 'int [2]') to 'S4'}} fxc-error {{X3017: cannot convert from 'typedef int[2]' to 'struct S4'}} */
    (S4)s2;                                                 /* expected-error {{cannot convert from 'S2' to 'S4'}} fxc-error {{X3017: cannot convert from 'struct S2' to 'struct S4'}} */
}