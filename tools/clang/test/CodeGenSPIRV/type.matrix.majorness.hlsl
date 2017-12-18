// Run: %dxc -T ps_6_0 -E main

// CHECK: 4:1: warning: row_major attribute for stand-alone matrix is not supported
row_major float2x3 grMajorMat;
// CHECK: 6:1: warning: column_major attribute for stand-alone matrix is not supported
column_major float2x3 gcMajorMat;

// CHECK: 9:8: warning: row_major attribute for stand-alone matrix is not supported
static row_major float2x3 gsrMajorMat;
// CHECK: 11:8: warning: column_major attribute for stand-alone matrix is not supported
static column_major float2x3 gscMajorMat;

void main() {
  // CHECK: 15:3: warning: row_major attribute for stand-alone matrix is not supported
  row_major float2x3 rMajorMat;
  // CHECK: 17:3: warning: column_major attribute for stand-alone matrix is not supported
  column_major float2x3 cMajorMat;
}
