// RUN: %dxc -T lib_6_3 -auto-binding-space 11 %s | FileCheck %s

// TODO: verify failure for exported library function and flip the CHECK below
// CHECK: define void @"\01?libfunc

struct Payload {
  float foo;
};
struct Attr {
  float2 bary;
};

void libfunc(inout Payload p, Attr a)  {
  float4 result = 2.6;
  if (p.foo < 2) {
    result.x = 1;             // unused
    AcceptHitAndEndSearch();  // does not return
    result.y = 3;             // dead code
  }
  if (a.bary.x < 9) {
    result.x = 2;             // unused
    IgnoreHit();              // does not return
    result.y = 4;             // dead code
  }
   p.foo = result;
}
