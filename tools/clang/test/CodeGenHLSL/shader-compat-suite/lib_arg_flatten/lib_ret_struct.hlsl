// RUN: %dxc -T lib_6_1 %s | FileCheck %s

// Make sure struct param used as out arg works.

// CHECK: call void @"\01?getT@@YA?AUT@@XZ"
// CHECK: store
// CHECK: store

struct T {
  float a;
  int   b;
};

T getT();

T getT2() {
  return getT();
}