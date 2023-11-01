// RUN: not %dxc -T cs_6_6 -E main -fcgl -spirv %s 2>&1 | FileCheck %s

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    Texture2D t = ResourceDescriptorHeap[0];
}

// CHECK: error: HLSL resource ResourceDescriptorHeap not yet supported with -spirv
