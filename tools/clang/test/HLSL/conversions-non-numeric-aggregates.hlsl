// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -verify -verify-ignore-unexpected=note %s

// Tests that conversions between numeric and non-numeric types/aggregates are disallowed.

struct NumStruct { int a; };
struct ObjStruct { Buffer a; };

void main()
{
  (Buffer[1])0; /* expected-error {{cannot convert from 'literal int' to 'Buffer [1]'}} */
  (ObjStruct)0; /* expected-error {{cannot convert from 'literal int' to 'ObjStruct'}} */
  (Buffer[1])(int[1])0; /* expected-error {{cannot convert from 'int [1]' to 'Buffer [1]'}} */
  (ObjStruct)(NumStruct)0; /* expected-error {{cannot convert from 'NumStruct' to 'ObjStruct'}} */

  Buffer oa1[1];
  ObjStruct os1;
  (int)oa1; /* expected-error {{cannot convert from 'Buffer [1]' to 'int'}} */
  (int)os1; /* expected-error {{cannot convert from 'ObjStruct' to 'int'}} */
}