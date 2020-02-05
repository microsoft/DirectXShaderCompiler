// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

// NOTE: The current debug info extension (OpenCL.DebugInfo.100
// Information Extended Instruction Set) does not support a matrix
// type. To avoid a crash from matrix type, we temporarily emit a
// debug array type for it. This test checks only that it runs
// without a crash.

// CHECK: {{%\d+}} = OpExtInst %void {{%\d+}} DebugTypeBasic {{%\d+}} %uint_32 Float

void main() {
   float3x4 mat;
}
