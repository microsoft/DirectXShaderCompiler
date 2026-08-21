// RUN: %dxc -T lib_6_3 %s -Fo %t.dxl
// RUN: %dxl %t.dxl -T gs_6_3 -E GS | FileCheck %s

// Make sure link succeeds and produces correct system value for SV_PrimitiveID
// i8 10 = DXIL::SemanticKind::PrimitiveID
// CHECK: !{i32 1, !"SV_PrimitiveID", i8 5, i8 10, !{{[0-9]+}}, i8 0, i32 1, i8 1, i32 -1, i8 -1, null}

struct GSINOUT {
    uint id : ID;
};

[maxvertexcount(3)]
[shader("geometry")]
void GS(point GSINOUT input[1],
        uint j : SV_PrimitiveID,
        inout PointStream<GSINOUT> outStream) {
    GSINOUT output = input[0];
    output.id += j;
    outStream.Append(output);
}
