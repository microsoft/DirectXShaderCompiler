// RUN: %dxc -T ps_6_8 -E main -Qkeep_reflect_in_dxil -select-validator internal %s | FileCheck -check-prefix=CHECK68 %s
// RUN: %dxc -T ps_6_7 -E main -Qkeep_reflect_in_dxil -select-validator internal %s | FileCheck -check-prefix=CHECK67 %s

// Make sure the vector is annotated with vector size (DXIL 1.8 and higher)
// 

// CHECK68: !dx.typeAnnotations

// CHECK68: !{i32 0, %"class.StructuredBuffer<MyStruct>" undef, !{{[0-9]+}}, %struct.MyStruct undef, [[ANNOT1:![0-9]+]]}
// CHECK68: [[ANNOT1:![0-9]+]] = !{i32 16, [[ANNOT2:![0-9]+]]}

// FieldAnnotationFieldNameTag(6) = "vec"
// FieldAnnotationCBufferOffsetTag(3) = 0
// FieldAnnotationCompTypeTag(7)= 0 (float)
// FieldAnnotationVectorSize(13) = 4
// CHECK68: [[ANNOT2:![0-9]+]] = !{i32 6, !"vec", i32 3, i32 0, i32 7, i32 9, i32 13, i32 4}

// CHECK67: !{i32 0, %"class.StructuredBuffer<MyStruct>" undef, !{{[0-9]+}}, %struct.MyStruct undef, [[ANNOT1:![0-9]+]]}
// CHECK67: [[ANNOT1:![0-9]+]] = !{i32 16, [[ANNOT2:![0-9]+]]}
// CHECK67: [[ANNOT2:![0-9]+]] = !{i32 6, !"vec", i32 3, i32 0, i32 7, i32 9}

struct MyStruct {
    float4 vec;
};

StructuredBuffer<MyStruct> g_myStruct;

float main() : SV_Target 
{ 
    return g_myStruct[0].vec.x + g_myStruct[0].vec.y; 
}