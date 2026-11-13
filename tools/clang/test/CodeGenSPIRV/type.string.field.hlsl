// RUN: %dxc -T cs_6_0 -E main -HV 2021 -spirv -verify %s
// RUN: %dxc -T cs_6_0 -E main -HV 202x -spirv -verify %s

struct StringState {
  string Text; // expected-error {{field of type string is not supported}}
};

template <typename T>
struct Wrapper {
  T Text; // expected-error {{field of type string is not supported}}
};

[numthreads(1, 1, 1)]
void main() {
  // expected-note@+1 {{in instantiation of template class 'Wrapper<const string>' requested here}}
  Wrapper<string> Wrapped;
}
