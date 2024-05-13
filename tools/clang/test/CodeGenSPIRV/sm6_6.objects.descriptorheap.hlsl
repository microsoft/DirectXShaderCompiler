// RUN: not %dxc -T cs_6_6 -E main -fcgl -spirv %s 2>&1 | FileCheck %s

[numthreads(32, 1, 1)]
void main() {
    Texture2D t = ResourceDescriptorHeap[0];
    SamplerState s = SamplerDescriptorHeap[0];
}

// CHECK: error: HLSL object ResourceDescriptorHeap not yet supported with -spirv
// CHECK: error: HLSL object SamplerDescriptorHeap not yet supported with -spirv
