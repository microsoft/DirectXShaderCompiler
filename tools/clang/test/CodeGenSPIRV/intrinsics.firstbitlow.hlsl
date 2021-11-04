// RUN: %dxc -T ps_6_0 -E main

// Note: Even though the HLSL documentation contains a version of "firstbitlow" that 
// takes signed integer(s) and returns signed integer(s), the frontend always generates
// the AST using the overloaded version that takes unsigned integer(s) and returns
// unsigned integer(s).

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  int   sint_1;
  int4  sint_4;
  uint  uint_1;
  uint4 uint_4;

// CHECK: {{%\d+}} = OpExtInst %uint [[glsl]] FindILsb {{%\d+}}
  int fbl =  firstbitlow(sint_1);

// CHECK: {{%\d+}} = OpExtInst %v4uint [[glsl]] FindILsb {{%\d+}}
  int4 fbl4 =  firstbitlow(sint_4);

// CHECK: {{%\d+}} = OpExtInst %uint [[glsl]] FindILsb {{%\d+}}
  uint ufbl =  firstbitlow(uint_1);

// CHECK: {{%\d+}} = OpExtInst %v4uint [[glsl]] FindILsb {{%\d+}}
  uint4 ufbl4 =  firstbitlow(uint_4);
}
