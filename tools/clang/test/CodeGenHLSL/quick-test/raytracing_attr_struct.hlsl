// RUN: %dxc -E main -T lib_6_3 %s | FileCheck %s

//CHECK: User define type intrinsic arg must be struct

float main(float THit : t, uint HitKind : h, float2 f2 : F) {
  return ReportHit(THit, HitKind, f2);
}