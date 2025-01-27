// RUN: %dxc -T lib_6_9 -E main %s

// TODO: Implement lowering for dx::HitObject::MakeNop

// CHECK-NOT: call

[shader("raygeneration")]
void main() {
  dx::HitObject hit;
  dx::HitObject::MakeNop();
}
