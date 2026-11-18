// RUN: %dxc -T lib_6_9 -HV 202x -verify %s

// SetShaderTableIndex mutates the dx::HitObject, so it must not be callable
// on a const-qualified instance. Const accessors should still work.

void use_const(const dx::HitObject ho) {
  // expected-error@+2{{no matching member function for call to 'SetShaderTableIndex'}}
  // expected-note@+1 1+ {{but method is not marked const}}
  ho.SetShaderTableIndex(1);

  // Const accessors are fine.
  bool isMiss = ho.IsMiss();
  bool isHit  = ho.IsHit();
  uint idx    = ho.GetShaderTableIndex();
  (void)isMiss; (void)isHit; (void)idx;
}

[shader("raygeneration")]
void main() {
  dx::HitObject ho;
  use_const(ho);
}
