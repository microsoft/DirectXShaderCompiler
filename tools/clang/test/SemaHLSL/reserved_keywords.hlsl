// RUN: %dxc -Tps_6_0 -verify %s

// expected-error@+2 {{'interface' is a reserved keyword in HLSL}}
// expected-error@+1 {{HLSL requires a type specifier for all declarations}}
interface I
{  // expected-warning {{effect state block ignored - effect syntax is deprecated. To use braces as an initializer use them with equal signs.}}
    void f();
};

// expected-error@+2 {{'union' is a reserved keyword in HLSL}}
// expected-error@+1 {{HLSL requires a type specifier for all declarations}}
union U
{  // expected-warning {{effect state block ignored - effect syntax is deprecated. To use braces as an initializer use them with equal signs.}}
    void f();
};

void main() {}
