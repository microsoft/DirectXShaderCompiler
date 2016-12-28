// RUN: %dxc -E main -T ps_5_0 %s | FileCheck %s

// CHECK: InnerCoverage and Coverage are mutually exclusive.

void main(snorm float b : B, float c:C, in uint inner : SV_InnerCoverage, inout uint cover: SV_Coverage)
{
}
