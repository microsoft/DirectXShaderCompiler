// RUN: %dxc -E main -enable-unions -T vs_6_2 %s | FileCheck %s

struct s0 {};
union s1 { s0 a; uint b; };

// CHECK: ret
s1 main() : OUT { s1 s; return s; }

