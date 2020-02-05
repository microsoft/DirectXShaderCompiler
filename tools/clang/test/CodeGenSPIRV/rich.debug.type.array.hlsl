// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// CHECK:       [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK:  [[uintName:%\d+]] = OpString "uint"
// CHECK:   [[intName:%\d+]] = OpString "int"
// CHECK: [[floatName:%\d+]] = OpString "float"
// CHECK:           %uint_32 = OpConstant %uint 32

void main() {
    const uint size = 4 * 3 - 4;

// CHECK:  [[uint:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[uintName]] %uint_32 Unsigned
// CHECK:       {{%\d+}} = OpExtInst %void [[set]] DebugTypeArray [[uint]] %uint_4
    uint  x[4];
// CHECK:   [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[intName]] %uint_32 Signed
// CHECK:       {{%\d+}} = OpExtInst %void [[set]] DebugTypeArray [[int]] %uint_8
    int   y[size];
// CHECK: [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic [[floatName]] %uint_32 Float
// CHECK:       {{%\d+}} = OpExtInst %void [[set]] DebugTypeArray [[float]] %uint_8 %uint_4
    float z[size][4];
}
