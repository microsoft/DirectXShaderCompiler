// RUN: %dxc -T lib_6_9 %s | FileCheck %s

// CHECK: error: max vertex count should not exceed 256

struct MeshPerVertex {
    float4 position : SV_Position;
    float color[4] : COLOR;
};

[Shader("node")]
[NodeLaunch("mesh")]
[outputtopology("line")]
[numthreads(128, 1, 1)]
[NodeDispatchGrid(64,1,1)]
void main(out vertices MeshPerVertex verts[257]) { }
