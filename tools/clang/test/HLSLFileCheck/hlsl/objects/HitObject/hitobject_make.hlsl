// RUN: %dxc -T lib_6_9 -E main %s

// TODO: Implement lowering for dx::HitObject::MakeNop

// CHECK: define void
// CHECK-NEXT: entry:
// CHECK-NEXT:   ret void
// CHECK-NEXT: }

[shader("raygeneration")]
void main() {
  dx::HitObject hit;
  dx::HitObject::MakeNop();
}
