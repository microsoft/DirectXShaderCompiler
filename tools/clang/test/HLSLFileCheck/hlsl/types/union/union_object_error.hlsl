// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s
union MyUnion {
  int Y;
  int X;
};

int a(MyUnion U : A) { // CHECK: error: union objects cannot have HLSL annotations applied to them
  return U.X;
}

int main() : OUT {
  MyUnion U;
  return a(U);
}
