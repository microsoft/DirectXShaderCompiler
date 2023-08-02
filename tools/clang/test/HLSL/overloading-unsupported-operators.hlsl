// RUN: %clang_cc1 -fsyntax-only -Wno-unused-value -ffreestanding -verify -HV 2021 %s

// This test checks that dxcompiler generates errors when overloading operators
// that are not supported for overloading in HLSL 2021

struct S
{
    float foo;
    void operator=(S s) {} // expected-error {{overloading 'operator=' is not allowed}}
    void operator+=(S s) {} // expected-error {{overloading 'operator+=' is not allowed}}
    void operator-=(S s) {} // expected-error {{overloading 'operator-=' is not allowed}}
    void operator*=(S s) {} // expected-error {{overloading 'operator*=' is not allowed}}
    void operator/=(S s) {} // expected-error {{overloading 'operator/=' is not allowed}}
    void operator%=(S s) {} // expected-error {{overloading 'operator%=' is not allowed}}
    void operator^=(S s) {} // expected-error {{overloading 'operator^=' is not allowed}}
    void operator&=(S s) {} // expected-error {{overloading 'operator&=' is not allowed}}
    void operator|=(S s) {} // expected-error {{overloading 'operator|=' is not allowed}}
    void operator<<(S s) {} // expected-error {{overloading 'operator<<' is not allowed}}
    void operator>>(S s) {} // expected-error {{overloading 'operator>>' is not allowed}}
    void operator<<=(S s) {} // expected-error {{overloading 'operator<<=' is not allowed}}
    void operator>>=(S s) {} // expected-error {{overloading 'operator>>=' is not allowed}}
    void operator->*(S s) {} // expected-error {{overloading 'operator->*' is not allowed}}
    void operator->(S s) {} // expected-error {{overloading 'operator->' is not allowed}}
    void operator++(S s) {} // expected-error {{overloading 'operator++' is not allowed}}
    void operator--(S s) {} // expected-error {{overloading 'operator--' is not allowed}}
};

[numthreads(1,1,1)]
void main() {}
