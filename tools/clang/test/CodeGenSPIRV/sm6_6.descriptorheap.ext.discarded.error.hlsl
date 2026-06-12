// RUN: not %dxc -T cs_6_6 -E main -Od -fspv-use-descriptor-heap -fspv-target-env=vulkan1.3 -spirv %s 2>&1 | FileCheck %s

// Regression test for an unguarded cast in doCXXOperatorCallExpr:
//   cast<CastExpr>(parentMap->getParent(expr))
// A descriptor-heap index whose result is discarded (used as a bare expression
// statement) has no parent cast to a concrete resource type, so the resource
// type cannot be determined. This must emit a diagnostic, not an ICE.

// CHECK: error: {{.*}}ResourceDescriptorHeap/SamplerDescriptorHeap indexing must be used as a resource

[numthreads(1, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID) {
  ResourceDescriptorHeap[tid.x];
}
