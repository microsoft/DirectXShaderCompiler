// RUN: %dxc -E main -T vs_6_0 %s | FileCheck %s

// CHECK: @main()
// CHECK-NOT: addrspacecast
// CHECK: ret void

struct Foo { int x; int getX() { return x; } };
groupshared Foo foo[2];
int i;
int main() : OUT { return foo[i].getX(); }
