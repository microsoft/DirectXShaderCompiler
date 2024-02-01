// RUN: %dxc -T lib_6_x %S\node-object-export-1.hlsl -Fo %t.1
// RUN: %dxc -T lib_6_x %s -Fo %t.2
// RUN: %dxc -T lib_6_x -link "%t.1;%t.2" | FileCheck %s


// CHECK: define void @"\01?foo2@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* noalias nocapture sret, %"struct.DispatchNodeInputRecord<RECORD>"* nocapture readonly) #0 {
// CHECK:   %[[Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, align 4
// CHECK:   store %"struct.DispatchNodeInputRecord<RECORD>" %[[Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, align 4

// CHECK: define void @"\01?bar@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* nocapture readonly, %"struct.DispatchNodeInputRecord<RECORD>"* noalias nocapture) #0 {
// CHECK:   %[[Alloca:.+]] = alloca %"struct.DispatchNodeInputRecord<RECORD>", align 8
// CHECK:   call void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* nonnull sret %[[Alloca]], %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}})
// CHECK:   %[[Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %[[Alloca]], align 8
// CHECK:   store %"struct.DispatchNodeInputRecord<RECORD>" %[[Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, align 4

// CHECK: define void @"\01?bar2@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* nocapture readonly, %"struct.DispatchNodeInputRecord<RECORD>"* noalias nocapture) #1 {
// CHECK:   %[[Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, align 4
// CHECK:   store %"struct.DispatchNodeInputRecord<RECORD>" %[[Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, align 4

// CHECK: define void @"\01?bar3@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"*, %"struct.DispatchNodeInputRecord<RECORD>"* noalias) #0 {
// CHECK:   %[[Alloca:.+]] = alloca %"struct.DispatchNodeInputRecord<RECORD>", align 8
// CHECK:   call void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* nonnull sret %[[Alloca]], %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}) #2
// CHECK:   %[[Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %[[Alloca]], align 8
// CHECK:   store %"struct.DispatchNodeInputRecord<RECORD>" %[[Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, align 4

// CHECK: define void @"\01?bar4@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"*, %"struct.DispatchNodeInputRecord<RECORD>"* noalias) #0 {
// CHECK:   call void @"\01?bar2@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, %"struct.DispatchNodeInputRecord<RECORD>"* %1) #2

// CHECK: define void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* noalias nocapture sret, %"struct.DispatchNodeInputRecord<RECORD>"* nocapture readonly) #1 {
// CHECK:   %[[Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, align 4
// CHECK:   store %"struct.DispatchNodeInputRecord<RECORD>" %[[Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %{{.+}}, align 4


struct RECORD {
  int X;
};


void bar(DispatchNodeInputRecord<RECORD> input, out DispatchNodeInputRecord<RECORD> output);

[noinline]
void bar2(DispatchNodeInputRecord<RECORD> input, out DispatchNodeInputRecord<RECORD> output);

export
void bar3(DispatchNodeInputRecord<RECORD> input, out DispatchNodeInputRecord<RECORD> output) {
  bar(input, output);
}

export
void bar4(DispatchNodeInputRecord<RECORD> input, out DispatchNodeInputRecord<RECORD> output) {
  bar2(input, output);
}

