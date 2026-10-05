// RUN: %dxc -T ps_6_0 -E main -HV 202x -verify %s

// Verify that inside a const-qualified instance method, 'this' refers to a
// const object and the object's fields cannot be modified. A non-const
// sibling method should still be able to mutate the same fields.

struct S {
  int x;
  int arr[4];

  void modify(int v) const {     // expected-note 2 {{member function 'S::modify' is declared const here}}
    x = v;                       // expected-error {{cannot assign to non-static data member within const member function 'modify'}}
    arr[0] = v;                  // expected-error {{read-only variable is not assignable}}
    x += v;                      // expected-error {{cannot assign to non-static data member within const member function 'modify'}}
  }

  void mutate(int v) {
    // Non-const method: mutation is fine.
    x = v;
    arr[0] = v;
  }

  int read() const {
    // Reading 'this' fields from a const method is fine.
    return x + arr[0];
  }
};

float4 main() : SV_Target {
  S s = {1, {2, 3, 4, 5}};
  s.mutate(7);
  return s.read();
}
