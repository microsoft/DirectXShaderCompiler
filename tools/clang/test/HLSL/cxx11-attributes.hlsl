// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

struct S {
    [[vk::location(1)]] // expected-warning {{'location' attribute ignored}}
    float a: A;
};

[[maybe_unused]] // expected-warning {{unknown attribute 'maybe_unused' ignored}}
float main([[scope::attr(0, "str")]] // expected-warning {{unknown attribute 'attr' ignored}}
           float m: B,
           S s) : C {
    return m + s.a;
}
