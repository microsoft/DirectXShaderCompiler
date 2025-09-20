// RUN: %dxc -T lib_6_5 -verify %s

// Check that a parenthesised list (or malformed version) following a
// semantic is not mis-parsed as a direct initializer.

StructuredBuffer<uint> foo : FOO(123);  // expected-error {{expected ';' after top level declarator}}

uint a : sema;

uint b : sema(   ;  // expected-error {{expected ';' after top level declarator}}

uint c : sema();  // expected-error {{expected ';' after top level declarator}}

uint d : sema(();  // expected-error {{expected ';' after top level declarator}}

uint e : sema  () (123);  // expected-error {{expected ';' after top level declarator}}

uint f : sema(9);  // expected-error {{expected ';' after top level declarator}}

uint g : sema(((100)));  // expected-error {{expected ';' after top level declarator}}

uint h : sema(1,2,(3));  // expected-error {{expected ';' after top level declarator}}

uint i : sema : sema(99);  // expected-error {{expected ';' after top level declarator}}
