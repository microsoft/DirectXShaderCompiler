// RUN: %dxc -E main -T vs_6_2 %s | FileCheck %s

// Repro of GitHub #1842

struct S1 { int a, b; };
struct S2 { int a, b; };
void foo(S2 s) {}
void main() { S1 s; foo(s); }