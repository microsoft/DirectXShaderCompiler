// RUN: %dxc -T lib_6_8 %s -verify

// Check that the HitObject type name of Shader Execution Reordering is unclaimed pre SM 6.9.
// expected-no-diagnostics

namespace dx {
struct HitObject {
  int notTheSM69HitObject;
  static HitObject MakeNop() {
    HitObject hit;
    hit.notTheSM69HitObject = 1;
    return hit;
  }
};
}

[shader("raygeneration")]
void main() {
  dx::HitObject hit = dx::HitObject::MakeNop();
}
