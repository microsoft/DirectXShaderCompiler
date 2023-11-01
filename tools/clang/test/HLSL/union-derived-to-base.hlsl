// RUN: %clang_cc1 -HV 202x -fsyntax-only -ffreestanding -verify %s
union Base {
    float4 a;
    float4 b;
};

union Derived : Base { /* expected-error {{unions cannot have base classes}} */
    float4 b;
    float4 c;
};
