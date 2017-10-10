// RUN: %dxc -E main -T ps_6_0 %s  | FileCheck %s

// CHECK: main

class A {
  float m_a;
  void bar() { m_a = 1.2; }
};
class B : A {
  int m_b;
  void foo() {
    m_a = 1.3;
    m_b = 3;
  }
};

float main() : SV_Target {
  B b;
  b.bar();
  b.foo();
  return b.m_a;
}