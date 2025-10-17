// RUN: %dxc -E main -HV 202x -T vs_6_2 %s | FileCheck %s
union MyUnion {
  uint x;
  float y : register(c1):abc; // CHECK: error: union members cannot have HLSL annotations applied to them
  float z : packoffset(c2); // CHECK: error: union members cannot have HLSL annotations applied to them
};

int main(MyUnion U
         : A) : SV_TARGET { // CHECK: error: union objects cannot have HLSL annotations applied to them
  return U.y;
}
