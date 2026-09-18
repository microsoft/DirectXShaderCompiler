// RUN: %dxc -T lib_6_3 -HV 202x -verify %s

// Verify unsupported constructs and standard variadic-template diagnostics.

// HLSL does not support multiple or variadic base classes.
struct Base1 {
  float x;
};
struct Base2 {
  float y;
};

template <typename... Bases>
struct Derived : Bases... {
  // expected-error@-1{{base type ellipsis is unsupported in HLSL}}
  // expected-error@-2{{multiple concrete base types specified}}
  float z;
};

void UseDerived() {
  Derived<Base1, Base2> d;
  // expected-note@-1{{in instantiation of template class 'Derived<Base1, Base2>' requested here}}
  d.z = 0;
}

// Class template parameter packs must remain last.
template <typename... Ts, typename U>
// expected-error@-1{{template parameter pack must be the last template parameter}}
struct PackNotLast {};

// sizeof...() only applies to the name of an actual parameter pack.
uint NotAPack() {
  return sizeof...(NotAPack);
  // expected-error@-1{{'NotAPack' does not refer to the name of a parameter pack}}
}

// C-style variadic functions remain unsupported.
void CStyleVarArgs(int a, ...);
// expected-error@-1{{variadic arguments is unsupported in HLSL}}

// Mismatched pack arguments use ordinary overload-resolution diagnostics.
template <typename... Ts>
struct Zipper {
  template <typename... Us>
  static uint Count(Ts... ts, Us... us) {
    // expected-note@-1{{candidate function not viable: requires 3 arguments, but 2 were provided}}
    return sizeof...(Ts) + sizeof...(Us);
  }
};

uint TestMismatchedPackArgs() {
  return Zipper<int, float>::Count<double>(1, 2.0);
  // expected-error@-1{{no matching function for call to 'Count'}}
}
