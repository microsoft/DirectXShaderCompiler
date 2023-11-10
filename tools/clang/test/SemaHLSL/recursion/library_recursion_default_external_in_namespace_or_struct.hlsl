// RUN: %dxc -T lib_6_3 -default-linkage external %s -verify

// This file tests that default-linkage external works, and applies
// external linkage to decls inside namespaces, structs, or classes
// It will find such decls, and perform recursion validation on them.

struct S
{
// expected-error@+1{{recursive functions are not allowed: export function calls recursive function 'func'}}
  int func() { return func(); }
};

namespace something {
// expected-error@+1{{recursive functions are not allowed: export function calls recursive function 'func2'}}
  int func2() { return func2(); }
}

class S2
{
// expected-error@+1{{recursive functions are not allowed: export function calls recursive function 'func3'}}
  int func3() { return func3(); }
};

export int main() : OUT
{
  S s;
  return s.func();
}