// RUN: %dxc -E main -T ds_6_2 -DTY=float %s | FileCheck %s -check-prefix=CHK_NO_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=float2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=bool3 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=uint2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=int2x2 %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=uint %s | FileCheck %s -check-prefix=CHK_ERR
// RUN: %dxc -E main -T ds_6_2 -DTY=min16uint %s | FileCheck %s -check-prefix=CHK_ERR


// CHK_NO_ERR: define void @main
// CHK_ERR: error: invalid type used for 'SV_InsideTessFactor' semantic

// HS PCF output
struct HsPcfOut {
  float  outTessFactor[4]   : SV_TessFactor;
  TY     inTessFactor[2]    : SV_InsideTessFactor;
  uint   index              : SV_RenderTargetArrayIndex;
};

// Per-vertex input structs
struct DsCpIn {
  uint   index              : SV_RenderTargetArrayIndex;
};

// Per-vertex output structs
struct DsCpOut {
  uint   index              : SV_RenderTargetArrayIndex;
};

[domain("quad")]
DsCpOut main(OutputPatch<DsCpIn, 3> patch, HsPcfOut pcfData) {
  DsCpOut dsOut;
  dsOut = (DsCpOut)0;
  return dsOut;
}