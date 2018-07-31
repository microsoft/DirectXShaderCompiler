// Run: %dxc -T ps_6_0 -E main -Oconfig=-O -Oconfig=-Os

void main() {}

// CHECK: -Oconfig should not be specified more than once