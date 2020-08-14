// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:       [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:  [[boolName:%\d+]] = OpString "bool"
// CHECK:     [[SName:%\d+]] = OpString "S"
// CHECK:   [[intName:%\d+]] = OpString "int"
// CHECK:  [[uintName:%\d+]] = OpString "uint"
// CHECK: [[floatName:%\d+]] = OpString "float"
// CHECK:           %uint_32 = OpConstant %uint 32

// CHECK:   [[bool:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[boolName]] %uint_32 Boolean
// CHECK: [[S:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[SName]]
// CHECK:   [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[intName]] %uint_32 Signed
// CHECK:  [[uint:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[uintName]] %uint_32 Unsigned
// CHECK: [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[floatName]] %uint_32 Float
// CHECK:        {{%\d+}} = OpExtInst %void [[set]] DebugTypeArray [[S]] %uint_8
// CHECK: [[boolv4:%\d+]] = OpExtInst %void [[set]] DebugTypeVector [[bool]] 4
// CHECK:        {{%\d+}} = OpExtInst %void [[set]] DebugTypeArray [[boolv4]] %uint_7
// CHECK:       {{%\d+}} = OpExtInst %void [[set]] DebugTypeArray [[float]] %uint_8 %uint_4
// CHECK:       {{%\d+}} = OpExtInst %void [[set]] DebugTypeArray [[int]] %uint_8
// CHECK:       {{%\d+}} = OpExtInst %void [[set]] DebugTypeArray [[uint]] %uint_4

void main() {
    const uint size = 4 * 3 - 4;

    uint  x[4];
    int   y[size];
    float z[size][4];
    bool4 v[7];

    struct S {
      uint a;
      int b;
      bool c;
    } w[size];
}
