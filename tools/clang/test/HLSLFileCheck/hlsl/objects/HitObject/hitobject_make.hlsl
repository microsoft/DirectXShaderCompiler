// RUN: %dxc -T lib_6_9 -E main %s | FileCheck %s
// REQUIRES: dxil-1-9

// TODO: Implement lowering for dx::HitObject::MakeNop

// CHECK-NOT: call

[shader("raygeneration")]
void main() {
  dx::HitObject hit;
  dx::HitObject::MakeNop();
  RayDesc ray = {{0,0,0}, {0,0,1}, 0.05, 1000.0};
  dx::HitObject::MakeMiss(0, 1, ray);
}
