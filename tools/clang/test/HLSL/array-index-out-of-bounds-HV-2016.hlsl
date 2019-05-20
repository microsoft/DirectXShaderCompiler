// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -HV 2016 -verify -verify-ignore-unexpected=note %s

void dead()
{
    int array[2];
    array[-1] = 0;                                          /* expected-warning {{array index -1 is before the beginning of the array}} fxc-pass */
    array[0] = 0;
    array[1] = 0;
    array[2] = 0;                                           /* expected-warning {{array index 2 is past the end of the array (which contains 2 elements)}} fxc-pass */
}

void main() {}