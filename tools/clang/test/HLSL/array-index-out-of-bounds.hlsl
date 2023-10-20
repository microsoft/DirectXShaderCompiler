// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -verify %s

void main()
{
    // expected-note@+2 {{array 'array' declared here}}
    // expected-note@+1 {{array 'array' declared here}}
    int array[2];
    array[-1] = 0;                                          /* expected-error {{array index -1 is out of bounds}} fxc-error {{X3504: array index out of bounds}} */
    array[0] = 0;
    array[1] = 0;
    array[2] = 0;                                           /* expected-error {{array index 2 is out of bounds}} fxc-error {{X3504: array index out of bounds}} */
}
