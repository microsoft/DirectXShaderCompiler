// RUN: %dxc -T cs_6_0 -HV 202x -verify %s

// Test diagnostics for incorrect uses of 'auto' as a function return type.

// Inconsistent deduced types between return statements is an error.
auto BadDeduction(int x) {
    if (x > 0)
        return 1; // deduced as int
    return 2.0f; // expected-error {{'auto' in return type deduced as 'float' here but deduced as 'int' in earlier return statement}}
}

// A function declared with 'auto' must be defined before it is used; a
// forward declaration alone is not sufficient for the call site.
auto ForwardOnly(int x);
void useForward() {
    ForwardOnly(1); // expected-error {{function 'ForwardOnly' with deduced return type cannot be used before it is defined}}
}
// expected-note@-4 {{'ForwardOnly' declared here}}

// A recursive call to a function with deduced return type must occur after
// a return statement that allows the type to be deduced.
auto BadRecurse(int x) {
    return BadRecurse(x - 1) + 1; // expected-error {{cannot be used before it is defined}}
}
// expected-note@-3 {{'BadRecurse' declared here}}

// Returning an initializer list as the deduced return value is not allowed.
// (Initializer lists in return statements are also unsupported in HLSL more
// broadly, but the auto-return-type case is still flagged early.)

[numthreads(1,1,1)]
void main() {
    BadDeduction(1);
    BadRecurse(2);
}
