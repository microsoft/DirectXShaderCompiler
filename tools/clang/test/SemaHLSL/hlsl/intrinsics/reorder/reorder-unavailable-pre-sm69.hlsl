// RUN: %dxc -T lib_6_8 %s -verify

// Check that intrinsic names of Shader Execution Reordering are unclaimed pre SM 6.9.

[shader("raygeneration")]
void main() {
  // expected-warning@+1{{potential misuse of built-in function 'dx::MaybeReorderThread' in shader model lib_6_8; introduced in shader model 6.9}}
  dx::MaybeReorderThread(15u, 4u);
}
