// Run: %dxc -T vs_6_0 -E main -fspv-debug=rich

//CHECK: [[name_2d_arr:%\d+]] = OpString "@type.2d.image.array"
//CHECK: [[name_1d_arr:%\d+]] = OpString "@type.1d.image.array"
//CHECK: [[name_3d:%\d+]] = OpString "@type.3d.image"
//CHECK: [[name_1d:%\d+]] = OpString "@type.1d.image"

//CHECK: [[tem_p6:%\d+]] = OpExtInst %void [[ext:%\d+]] DebugTypeTemplateParameter
//CHECK: [[2daf:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name_2d_arr]] Class [[src:%\d+]] 0 0 [[cu:%\d+]] {{%\d+}} [[info_none:%\d+]]
//CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[2daf]] [[tem_p6]]

//CHECK: [[tem_p3:%\d+]] = OpExtInst %void [[ext]] DebugTypeTemplateParameter
//CHECK: [[1dai:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name_1d_arr]] Class [[src]] 0 0 [[cu]] {{%\d+}} [[info_none]]
//CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[1dai]] [[tem_p3]]

//CHECK: [[tem_p2:%\d+]] = OpExtInst %void [[ext]] DebugTypeTemplateParameter
//CHECK: [[3df:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name_3d]] Class [[src]] 0 0 [[cu]] {{%\d+}} [[info_none]]
//CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[3df]] [[tem_p2]]

//CHECK: [[tem_p0:%\d+]] = OpExtInst %void [[ext]] DebugTypeTemplateParameter
//CHECK: [[1di:%\d+]] = OpExtInst %void [[ext]] DebugTypeComposite [[name_1d]] Class [[src]] 0 0 [[cu]] {{%\d+}} [[info_none]]
//CHECK: OpExtInst %void [[ext]] DebugTypeTemplate [[1di]] [[tem_p0]]

//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[2daf]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %t8
//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[1dai]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %t5
//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[3df]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %t3
//CHECK: OpExtInst %void [[ext]] DebugGlobalVariable {{%\d+}} [[1di]] [[src]] {{\d+}} {{\d+}} [[cu]] {{%\d+}} %t1

RWTexture1D   <int>    t1 ;
RWTexture3D   <float3> t3 ;
RWTexture1DArray<int>    t5;
RWTexture2DArray<float4> t8;

void main() {}
