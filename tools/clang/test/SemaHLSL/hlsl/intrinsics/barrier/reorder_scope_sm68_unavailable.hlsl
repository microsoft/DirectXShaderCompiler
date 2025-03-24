// RUN: %dxc -Tlib_6_8 -verify %s

[Shader("compute")]
[numthreads(1, 1, 1)]
void main() {
  // expected-warning@+1{{potential misuse of built-in constant 'REORDER_SCOPE' in shader model lib_6_8; introduced in shader model 6.9}}
  Barrier(0, REORDER_SCOPE);
}
