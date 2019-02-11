// RUN: %dxc -E main -T vs_6_2 %s | FileCheck %s

// Repro of GitHub #1843

struct Base {};
struct Derived : Base { int a; };
void f(Base b) {}
void main() { Derived d; f(d); }