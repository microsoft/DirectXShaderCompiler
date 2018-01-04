// Run: %dxc -Zpr -T ps_6_0 -E main -spirv

cbuffer Buffer {
  //CHECK: OpMemberDecorate %type_Buffer 0 ColMajor
  row_major float2x3 grMajorMat;
  //CHECK: OpMemberDecorate %type_Buffer 1 RowMajor
  column_major float2x3 gcMajorMat;
  //CHECK: OpMemberDecorate %type_Buffer 2 ColMajor
  float2x3 gdMajorMat;
}

void main() {
}
