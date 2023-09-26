// RUN: %dxc -T ps_6_8 -E main -Qkeep_reflect_in_dxil -select-validator internal %s | FileCheck %s

// Make sure the vector is annotated with vector size (DXIL 1.8 and higher)

// CHECK: !dx.typeAnnotations

// CHECK: !{i32 0, %"class.StructuredBuffer<MyStruct>" undef, !8, %struct.MyStruct undef, [[ANNOT1:![0-9]+]]}
// CHECK: [[ANNOT1:![0-9]+]] = !{i32 16, [[ANNOT2:![0-9]+]]}
// CHECK: [[ANNOT2:![0-9]+]] = !{i32 6, !"vec", i32 3, i32 0, i32 7, i32 9, i32 13, i32 4}

struct MyStruct {
    float4 vec;
};

StructuredBuffer<MyStruct> g_myStruct;

float main() : SV_Target 
{ 
    return g_myStruct[0].vec.x + g_myStruct[0].vec.y; 
}