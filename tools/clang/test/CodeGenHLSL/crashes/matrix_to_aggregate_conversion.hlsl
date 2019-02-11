// RUN: %dxc -E main -T vs_6_2 %s | FileCheck %s

// Repro of GitHub #1799

typedef int ix4[4];
void f(ix4) {}
void main() { f((ix4)int2x2(1,2,3,4)); }