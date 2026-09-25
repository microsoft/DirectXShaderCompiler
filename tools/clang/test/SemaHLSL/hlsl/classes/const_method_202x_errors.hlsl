// RUN: %dxc -T ps_6_0 -E main -HV 202x -verify %s

// Verify that const-correctness is enforced for HLSL 202x: a non-const
// instance method cannot be called on a const object, regardless of whether
// the const-ness comes from a cbuffer member, a ConstantBuffer<T>, the
// implicit global cbuffer, an explicit `const` local, or `this` inside a
// const method.

struct S {
  int x;
  int get() const { return x; }
  int getNC() { return x; } // expected-note 6 {{'getNC' declared here}}

  int callNC() const {
    return getNC(); // expected-error {{member function 'getNC' not viable: 'this' argument has type 'const S', but function is not marked const}}
  }
  int callNCThis() const {
    return this.getNC(); // expected-error {{member function 'getNC' not viable: 'this' argument has type 'const S', but function is not marked const}}
  }

  int dup() const const { return x; } // expected-warning {{duplicate 'const' declaration specifier}}

  static int staticConst() const { return 0; } // expected-error {{static member function cannot have 'const' qualifier}}
};

template <typename T> struct W {
  T v;
  T get() const { return v; }
  T getNC() { return v; } // expected-note {{'getNC' declared here}}
};

int freeConst() const { return 0; } // expected-error {{non-member function cannot have 'const' qualifier}}

// `const` is only accepted on function declarators, not in array bounds.
static int Arr[const 4]; // expected-error {{expected expression}}

cbuffer CB {
  S cs;
};

ConstantBuffer<S> cb;

S g; // implicit global cbuffer member - implicitly const.

float4 main() : SV_Target {
  // OK: const method on each kind of const object.
  int a = cs.get();
  int b = cb.get();
  int c = g.get();
  const S ls = {1};
  int d = ls.get();
  const W<int> lw = {2};
  int j = lw.get();

  // Error: non-const method on const object.
  int e = cs.getNC(); // expected-error {{member function 'getNC' not viable: 'this' argument has type 'const S', but function is not marked const}}
  int f = cb.getNC(); // expected-error {{member function 'getNC' not viable: 'this' argument has type 'const S', but function is not marked const}}
  int h = g.getNC();  // expected-error {{member function 'getNC' not viable: 'this' argument has type 'const S', but function is not marked const}}
  int i = ls.getNC(); // expected-error {{member function 'getNC' not viable: 'this' argument has type 'const S', but function is not marked const}}
  int k = lw.getNC(); // expected-error {{member function 'getNC' not viable: 'this' argument has type 'const W<int>', but function is not marked const}}

  return float4(a, b, c, d) + float4(e, f, h, i) + j + k;
}
