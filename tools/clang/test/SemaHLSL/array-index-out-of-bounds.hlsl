// RUN: %dxc -Tlib_6_3 -Wno-unused-value -verify -verify-ignore-unexpected=note %s

void main()
{
    int array[2];
    array[-1] = 0;                                          /* expected-error {{array index -1 is out of bounds}} fxc-error {{X3504: array index out of bounds}} */
    array[0] = 0;
    array[1] = 0;
    array[2] = 0;                                           /* expected-error {{array index 2 is out of bounds}} fxc-error {{X3504: array index out of bounds}} */
}