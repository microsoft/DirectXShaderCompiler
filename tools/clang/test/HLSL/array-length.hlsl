// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify -HV 2016  %s

float4 planes1[8];
float4 planes2[3+2]; 

struct S {
    float4 planes[2]; 
};

[RootSignature("CBV(b0, space=0, visibility=SHADER_VISIBILITY_ALL)")]
float main(S s:POSITION) : SV_Target {
    float4 planes3[] = {{ 1.0, 2.0, 3.0, 4.0 }};
    
    int total = planes1.Length;	// expected-warning {{Length is deprecated}} fxc-pass {{}}
    total += planes2.Length;  	// expected-warning {{Length is deprecated}} fxc-pass {{}}
    total += planes3.Length;    // expected-warning {{Length is deprecated}} fxc-pass {{}}
    total += s.planes.Length;   // expected-warning {{Length is deprecated}} fxc-pass {{}}

    return total;
}