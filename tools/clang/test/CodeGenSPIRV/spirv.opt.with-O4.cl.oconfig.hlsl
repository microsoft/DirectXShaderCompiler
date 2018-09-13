// Run: %dxc -T ps_6_0 -E main -O4 -Oconfig=--loop-unroll

void main() {}

// CHECK: -Oconfig should not be used together with -O
