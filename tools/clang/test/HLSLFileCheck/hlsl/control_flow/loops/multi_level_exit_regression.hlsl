// RUN: %dxc -E main -T cs_6_0 %s | FileCheck %s
// CHECK: error: Loop must have break

RWByteAddressBuffer g_1 : register(u0);

void f() {
  uint l = 0u;
  while (true) {
    bool _e10 = (l < 512u);
    while (true) {
      if ((l != 9u)) {
        while (true) {
          if (!(_e10)) {
            return;
          }
          if (_e10) {
            {
              if (_e10) { break; }
            }
            continue;
          }
          {
            if (_e10) { break; }
          }
        }
        continue;
      }
      if (_e10) {
        break;
      } else {
        return;
      }
    }
    uint _e22 = dot(g_1.Load3(0u), g_1.Load3(0u));
    g_1.Store((4u * l), asuint(l));
    l = (l + 1u);
  }
}

[numthreads(1, 1, 1)]
void main() {
  while (true) {
    f();
    {
      if (false) { break; }
    }
  }
  return;
}
