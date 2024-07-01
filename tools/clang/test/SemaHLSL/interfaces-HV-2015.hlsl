// RUN: %dxc -Tlib_6_3  -verify -HV 2015 %s

interface my_interface { };

class my_class { };
class my_class_3 : my_interface { };
struct my_struct { };
struct my_struct_4 : my_interface { };
struct my_struct_5 : my_class, my_interface { };

struct my_struct_6 : my_class, my_interface, my_struct { }; // expected-error {{multiple concrete base types specified}}

interface my_interface_2 : my_interface { }; // expected-error {{interfaces cannot inherit from other types}}