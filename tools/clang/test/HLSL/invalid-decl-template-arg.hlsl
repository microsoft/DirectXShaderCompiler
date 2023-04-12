// RUN: %clang_cc1 -fsyntax-only -verify %s

struct FOOO_A_B { // expected-note {{'FOOO_A_B' declared here}} expected-note {{definition of 'FOOO_A_B' is not complete until the closing '}'}} fxc-pass {{}}
  FOOO_A v0; // expected-error {{field has incomplete type 'FOOO_A_B}} expected-error {{unknown type name 'FOOO_A'; did you mean 'FOOO_A_B'}} fxc-error {{X3000: unrecognized identifier 'FOOO_A'}}
};

RWStructuredBuffer<FOOO_A_B> Input;                         /* fxc-error {{X3037: object's templated type must have at least one element}} */

void main() {}
