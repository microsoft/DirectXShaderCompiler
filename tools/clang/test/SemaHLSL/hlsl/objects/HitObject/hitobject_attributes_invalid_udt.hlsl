// RUN: %dxc -T lib_6_9 -E main %s -verify

struct
CustomAttrs {
  vector<float, 32> v;
  RWStructuredBuffer<float> buf;
};

[shader("raygeneration")]
void main() {
  dx::HitObject hit;
  // expected-error@+1{{attributes type must be a user-defined type composed of only numeric types}}
  CustomAttrs attrs = hit.GetAttributes<CustomAttrs>();
}
