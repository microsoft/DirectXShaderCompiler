// Run: %dxc -T vs_6_0 -E main -fspv-debug=rich

//CHECK: [[name_2d_arr:%\d+]] = OpString "@type.2d.image.array"
//CHECK: [[name_2d:%\d+]] = OpString "@type.2d.image"
//CHECK: [[name_cb_arr:%\d+]] = OpString "@type.cube.image.array"

//CHECK: [[tem_p3:%\d+]] = OpExtInst %void [[ext:%\d+]] DebugTypeTemplateParameter
//CHECK: [[info_none:%\d+]] = OpExtInst %void [[ext]] DebugInfoNone
//CHECK: [[t3_ty:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name_2d_arr]] Class [[src:%\d+]] 0 0 [[cu:%\d+]] {{%\d+}} [[info_none]]
//CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[t3_ty]] [[tem_p3]]

//CHECK: [[tem_p2:%\d+]] = OpExtInst %void [[ext]] DebugTypeTemplateParameter
//CHECK: [[t2_ty:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name_2d]] Class [[src]] 0 0 [[cu]] {{%\d+}} [[info_none]]
//CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[t2_ty]] [[tem_p2]]

//CHECK: [[tem_p1:%\d+]] = OpExtInst %void [[ext]] DebugTypeTemplateParameter
//CHECK: [[t1_ty:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name_cb_arr]] Class [[src]] 0 0 [[cu]] {{%\d+}} [[info_none]]
//CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[t1_ty]] [[tem_p1]]

//CHECK: [[tem_p0:%\d+]] = OpExtInst %void [[ext]] DebugTypeTemplateParameter
//CHECK: [[t0_ty:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name_2d]] Class [[src]] 0 0 [[cu]] {{%\d+}} [[info_none]]
//CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[t0_ty]] [[tem_p0]]

//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[t3_ty]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %t3
//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[t2_ty]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %t2
//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[t1_ty]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %t1
//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[t0_ty]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %t0

Texture2D   <int4>   t0 : register(t0);
TextureCubeArray <float4> t1 : register(t1);
Texture2DMS      <int3>   t2 : register(t2);

struct S {
    float a;
    float b;
};

Texture2DArray<S> t3;

void main() {
}
