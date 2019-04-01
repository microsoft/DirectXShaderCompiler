// RUN: %dxc -E main -T ps_6_0 -Zi %s | FileCheck %s


// Make sure debug info works for flattened alloca.
// CHECK:call void @llvm.dbg.declare(metadata [2 x float]* %a2.1, 

struct X {
   float a;
   int b;
};

struct A {
  X x[25];
  float y[2];
};

A a;
float b;

void set(A aa) {
   aa = a;
   aa.y[0] = b;
   aa.y[1] = 3;
}

float4 main(uint l:L) : SV_Target {
  A a2;
  set(a2);
  return a2.x[l].a + a2.y[l];
}