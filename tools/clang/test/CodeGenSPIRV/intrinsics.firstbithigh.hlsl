// Run: %dxc -T ps_6_0 -E main

// Note: Even though the HLSL documentation contains a version of "firstbithigh" that 
// takes signed integer(s) and returns signed integer(s), the frontend always generates
// the AST using the overloaded version that takes unsigned integer(s) and returns
// unsigned integer(s). Therefore "FindSMsb" is not generated in any case below.

// CHECK: [[glsl:%\d+]] = OpExtInstImport "GLSL.std.450"

void main() {
  int   sint_1;
  int4  sint_4;
  uint  uint_1;
  uint4 uint_4;

// CHECK: {{%\d+}} = OpExtInst %uint [[glsl]] FindUMsb {{%\d+}}
  int fbh = firstbithigh(sint_1);

// CHECK: {{%\d+}} = OpExtInst %v4uint [[glsl]] FindUMsb {{%\d+}}
  int4 fbh4 = firstbithigh(sint_4);

// CHECK: {{%\d+}} = OpExtInst %uint [[glsl]] FindUMsb {{%\d+}}
  uint ufbh = firstbithigh(uint_1);

// CHECK: {{%\d+}} = OpExtInst %v4uint [[glsl]] FindUMsb {{%\d+}}
  uint4 ufbh4 = firstbithigh(uint_4);
}
