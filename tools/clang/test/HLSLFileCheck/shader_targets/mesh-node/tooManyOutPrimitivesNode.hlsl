// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// CHECK: error: max primitive count should not exceed 256

struct MeshPerPrimitive {
    float normal : NORMAL;
    float malnor : MALNOR;
    int layer[4] : LAYER;
};

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("line")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void main(out primitives MeshPerPrimitive prims[257]) { }
