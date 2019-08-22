// RUN: %clang_cc1 -verify %s

// This file contains scenarios to test warnings are generated when uninitialized
// variables or structs are used.

struct MyStruct
{
 float a;
 int b;
};

cbuffer MyCBuf
{
	float c;
}

uint uninitvar()
{
	uint sum; /* expected-note {{initialize the variable 'sum' to silence this warning}} */
    sum++; /* expected-warning {{variable 'sum' is uninitialized when used here}} */
	return sum;
}

uint uninitvarself()
{
	uint sum = sum; /* expected-warning {{variable 'sum' is uninitialized when used within its own initialization}} */
	return sum;
}

uint mayuninit(uint i)
{
	uint s; /* expected-note {{initialize the variable 's' to silence this warning}} */
	if(i > 10) /* expected-warning {{variable 's' is used uninitialized whenever 'if' condition is false}} expected-note {{remove the 'if' if its condition is always true}} */
	{
		s = 10;
	}
	return s; /* expected-note {{uninitialized use occurs here}} */
}

uint uninit2(uint s)
{
	s = s << 4;
	return s;
}

void main(uint i : IN){
	uint s; /* expected-note {{initialize the variable 's' to silence this warning}} */
	(void) uninit2(s); /* expected-warning {{variable 's' is uninitialized when used here}} */
	(void) uninitvar();
	(void) uninitvarself();
	MyStruct val1,val2; /* expected-note {{initialize the variable 'val1' to silence this warning}} */
	val2 = val1; /* expected-warning {{variable 'val1' is uninitialized when used here}} */
	mayuninit(i);
	float d = c;
}
