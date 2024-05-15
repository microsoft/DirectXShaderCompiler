// RUN: %dxc -T cs_6_6 %s | FileCheck %s

// This was resulting in an ASAN use-after-free in SimplifyCFG
// during SimplifyTerminatorOnSelect because it was deleting
// a PHI node with an input value that the pass later emits
// (the select condition value).
// We simply make sure this compiles here, but see
// simplifycfg-uaf-select-condition.ll for the IR-level test.

// CHECK: @main()

ByteAddressBuffer buff : register(t0);

[numthreads(1, 1, 1)]
void main() {
  if (buff.Load(0u)) {
    return;
  }

  int i = 0;
  int j = 0;
  while (true) {
    bool a = (i < 2);
    switch(i) {
      case 0: {
        while (true) {
          bool b = (j < 2);
          if (b) {
          } else {
            break;
          }
          while (true) {
            int unused = 0;
            while (true) {
              if (a) break;
            }
            while (true) {
              while (true) {
                if (b) {
                  if (b) return;
                } else {
                  break;
                }
                while (true) {
                  i = 0;
                  if (b) break;
                }
                if (a) break;
              }
              if (a) break;
            }
            if (a) break;
          }
          j = (j + 2);
        }
      }
    }
  }
}
