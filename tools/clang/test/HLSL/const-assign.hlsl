// RUN: %clang_cc1 -fsyntax-only -Wno-unused-value  -ffreestanding -verify %s

float4 main() : SV_TARGET
{
    const float c = 2.0; // expected-note {{variable 'c' declared const here}} expected-note {{variable 'c' declared const here}}
    c = c + 3.0; // expected-error {{cannot assign to variable 'c' with const-qualified type 'const float'}}
    c += 3.0; // expected-error {{cannot assign to variable 'c' with const-qualified type 'const float'}}
    return (float4)c;
}