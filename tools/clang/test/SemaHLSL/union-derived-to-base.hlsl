// RUN: %dxc -Tcs_6_3 -HV 202x -verify %s
union Base {
    float4 a;
    float4 b;
};

union Derived : Base { /* expected-error {{unions cannot have base classes}} */
    float4 b;
    float4 c;
};
