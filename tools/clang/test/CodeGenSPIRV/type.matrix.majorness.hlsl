// Run: %dxc -T ps_6_0 -E main

// CHECK: warning: row_major attribute for stand-alone matrix is not supported
row_major float2x3 grMajorMat;
// CHECK: warning: column_major attribute for stand-alone matrix is not supported
column_major float2x3 gcMajorMat;

// CHECK: warning: row_major attribute for stand-alone matrix is not supported
static row_major float2x3 gsrMajorMat;
// CHECK: warning: column_major attribute for stand-alone matrix is not supported
static column_major float2x3 gscMajorMat;

void main() {
  // CHECK: warning: row_major attribute for stand-alone matrix is not supported
  row_major float2x3 rMajorMat;
  // CHECK: warning: column_major attribute for stand-alone matrix is not supported
  column_major float2x3 cMajorMat;
}
