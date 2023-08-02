// RUN: %dxc -T ps_6_0 -E main -fspv-debug=rich -O3

void main() {
}
// CHECK:     OpLine {{%\d+}} 4 1
// CHECK-NOT: OpLine
