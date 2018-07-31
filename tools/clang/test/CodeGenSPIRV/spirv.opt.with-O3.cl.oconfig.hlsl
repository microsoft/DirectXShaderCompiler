// Run: %dxc -T ps_6_0 -E main -O3 -Oconfig=--loop-unroll

void main() {}

// CHECK: -Oconfig should not be used together with -O
