// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s
union MyUnion {
  int Y;
  int X : abc; // CHECK: error: union members cannot have HLSL semantics applied to them
};

int main(MyUnion U) {
  return U.X;
}
