// RUN: %dxc -T lib_6_x -fcgl %s | FileCheck %s  --check-prefix=FCGL
// RUN: %dxc -T lib_6_x -Zi %s | FileCheck %s  --check-prefix=DBG
// RUN: %dxc -T lib_6_x %s | FileCheck %s  --check-prefix=O3
// RUN: %dxc -T lib_6_x -Od %s | FileCheck %s  --check-prefix=Od

struct RECORD {
  int X;
};

// DBG: !DISubprogram(name: "foo", linkageName: "\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z", scope: !1, file: !1, line: {{[0-9]+}}, type: ![[FooTy:[0-9]+]], isLocal: false, isDefinition: true, scopeLine: {{[0-9]+}}, flags: DIFlagPrototyped, isOptimized: false, function: void (%"struct.DispatchNodeInputRecord<RECORD>"*, %"struct.DispatchNodeInputRecord<RECORD>"*)* @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z")
// DBG: ![[FooTy]] = !DISubroutineType(types: ![[FooTys:[0-9]+]])
// DBG: ![[FooTys]] = !{![[ObjTy:[0-9]+]], ![[ObjTy]]}
// DBG: ![[ObjTy]] = !DICompositeType(tag: DW_TAG_structure_type, name: "DispatchNodeInputRecord<RECORD>", file: !1, size: 32, align: 32, elements: !2, templateParams: ![[TemplateParams:[0-9]+]])
// DBG: ![[TemplateParams]] = !{![[TemplateParam:[0-9]+]]}
// DBG: ![[TemplateParam]] = !DITemplateTypeParameter(name: "recordtype", type: ![[RECORD:[0-9]+]])
// DBG: ![[RECORD]] = !DICompositeType(tag: DW_TAG_structure_type, name: "RECORD", file: !1, line: {{[0-9]+}}, size: 32, align: 32, elements: ![[RecordElts:[0-9]+]])
// DBG: ![[RecordElts]] = !{![[RecordElt:[0-9]+]]}
// DBG: ![[RecordElt]] = !DIDerivedType(tag: DW_TAG_member, name: "X", scope: ![[RECORD]], file: !1, line: {{[0-9]+}}, baseType: ![[INT:[0-9]+]], size: 32, align: 32)
// DBG: ![[INT]] = !DIBasicType(name: "int", size: 32, align: 32, encoding: DW_ATE_signed)
// DBG: !DISubprogram(name: "bar", linkageName: "\01?bar@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z", scope: !1, file: !1, line: {{[0-9]+}}, type: ![[BarTy:[0-9]+]], isLocal: false, isDefinition: true, scopeLine: {{[0-9]+}}, flags: DIFlagPrototyped, isOptimized: false, function: void (%"struct.DispatchNodeInputRecord<RECORD>"*, %"struct.DispatchNodeInputRecord<RECORD>"*)* @"\01?bar@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z")
// DBG: ![[BarTy]] = !DISubroutineType(types: ![[BarTys:[0-9]+]])
// DBG: ![[BarTys]] = !{null, ![[ObjTy]], ![[OutObjTy:[0-9]+]]}
// DBG: ![[OutObjTy]] = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: ![[ObjTy]])
// DBG: !DISubprogram(name: "foo2", linkageName: "\01?foo2@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z", scope: !1, file: !1, line: {{[0-9]+}}, type: ![[FooTy]], isLocal: false, isDefinition: true, scopeLine: {{[0-9]+}}, flags: DIFlagPrototyped, isOptimized: false, function: void (%"struct.DispatchNodeInputRecord<RECORD>"*, %"struct.DispatchNodeInputRecord<RECORD>"*)* @"\01?foo2@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z")
// DBG: !DISubprogram(name: "bar2", linkageName: "\01?bar2@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z", scope: !1, file: !1, line: {{[0-9]+}}, type: ![[BarTy]], isLocal: false, isDefinition: true, scopeLine: {{[0-9]+}}, flags: DIFlagPrototyped, isOptimized: false, function: void (%"struct.DispatchNodeInputRecord<RECORD>"*, %"struct.DispatchNodeInputRecord<RECORD>"*)* @"\01?bar2@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z")

// FCGL:define void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* noalias sret %agg.result, %"struct.DispatchNodeInputRecord<RECORD>"* %input)
// O3: define void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* noalias nocapture sret %agg.result, %"struct.DispatchNodeInputRecord<RECORD>"* nocapture readonly %input)
// Od: define void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* noalias sret %agg.result, %"struct.DispatchNodeInputRecord<RECORD>"* %input)
[noinline]
DispatchNodeInputRecord<RECORD> foo(DispatchNodeInputRecord<RECORD> input) {
// FCGL:  %[[FooLd:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %input
// FCGL:  store %"struct.DispatchNodeInputRecord<RECORD>" %[[FooLd]], %"struct.DispatchNodeInputRecord<RECORD>"* %agg.result
// O3:  %[[FooLd:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %input, align 4
// O3:  store %"struct.DispatchNodeInputRecord<RECORD>" %[[FooLd]], %"struct.DispatchNodeInputRecord<RECORD>"* %agg.result, align 4
// Od: %[[FooLd:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %input
// Od:   store %"struct.DispatchNodeInputRecord<RECORD>" %[[FooLd]], %"struct.DispatchNodeInputRecord<RECORD>"* %agg.result

  return input;
}

// FCGL: define void @"\01?bar@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* %input, %"struct.DispatchNodeInputRecord<RECORD>"* noalias %output)
// O3: define void @"\01?bar@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* nocapture readonly %input, %"struct.DispatchNodeInputRecord<RECORD>"* noalias nocapture %output)
// Od: define void @"\01?bar@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* %input, %"struct.DispatchNodeInputRecord<RECORD>"* noalias %output)
export
void bar(DispatchNodeInputRecord<RECORD> input, out DispatchNodeInputRecord<RECORD> output) {

// FCGL:  %[[TMP:.+]] = alloca %"struct.DispatchNodeInputRecord<RECORD>", align 4
// FCGL:  call void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* sret %[[TMP]], %"struct.DispatchNodeInputRecord<RECORD>"* %input)
// FCGL:  %[[BarLd:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %[[TMP]]
// FCGL:  store %"struct.DispatchNodeInputRecord<RECORD>" %[[BarLd]], %"struct.DispatchNodeInputRecord<RECORD>"* %output
// O3: %[[TMP:.+]] = alloca %"struct.DispatchNodeInputRecord<RECORD>", align 8
// O3: call void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* nonnull sret %[[TMP]], %"struct.DispatchNodeInputRecord<RECORD>"* %input)
// O3: %[[BarLd:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %[[TMP]], align 8
// O3: store %"struct.DispatchNodeInputRecord<RECORD>" %[[BarLd]], %"struct.DispatchNodeInputRecord<RECORD>"* %output, align 4
// Od:   %[[TMP:.+]] = alloca %"struct.DispatchNodeInputRecord<RECORD>", align 4
// Od:   call void @"\01?foo@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* sret %[[TMP]], %"struct.DispatchNodeInputRecord<RECORD>"* %input)
// Od:   %[[BarLd:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %[[TMP]]
// Od:   store %"struct.DispatchNodeInputRecord<RECORD>" %[[BarLd]], %"struct.DispatchNodeInputRecord<RECORD>"* %output
  output = foo(input);
}

// FCGL: define void @"\01?foo2@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* noalias sret %agg.result, %"struct.DispatchNodeInputRecord<RECORD>"* %input)
// O3: define void @"\01?foo2@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* noalias nocapture sret %agg.result, %"struct.DispatchNodeInputRecord<RECORD>"* nocapture readonly %input)
// Od: define void @"\01?foo2@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* noalias sret %agg.result, %"struct.DispatchNodeInputRecord<RECORD>"* %input)
DispatchNodeInputRecord<RECORD> foo2(DispatchNodeInputRecord<RECORD> input) {
// FCGL: %[[Foo2Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %input
// FCGL: store %"struct.DispatchNodeInputRecord<RECORD>" %[[Foo2Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %agg.result
// O3: %[[Foo2Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %input, align 4
// O3: store %"struct.DispatchNodeInputRecord<RECORD>" %[[Foo2Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %agg.result, align 4
// Od: %[[Foo2Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %input
// Od: store %"struct.DispatchNodeInputRecord<RECORD>" %[[Foo2Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %agg.result
  return input;
}

// FCGL: define void @"\01?bar2@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* %input, %"struct.DispatchNodeInputRecord<RECORD>"* noalias %output)
// O3: define void @"\01?bar2@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* nocapture readonly %input, %"struct.DispatchNodeInputRecord<RECORD>"* noalias nocapture %output)
// Od: define void @"\01?bar2@@YAXU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* %input, %"struct.DispatchNodeInputRecord<RECORD>"* noalias %output)
[noinline]
export
void bar2(DispatchNodeInputRecord<RECORD> input, out DispatchNodeInputRecord<RECORD> output) {
// FCGL: %[[TMP:.+]] = alloca %"struct.DispatchNodeInputRecord<RECORD>", align 4
// FCGL: call void @"\01?foo2@@YA?AU?$DispatchNodeInputRecord@URECORD@@@@U1@@Z"(%"struct.DispatchNodeInputRecord<RECORD>"* sret %[[TMP]], %"struct.DispatchNodeInputRecord<RECORD>"* %input)
// FCGL: %[[Bar2Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %[[TMP]]
// FCGL: store %"struct.DispatchNodeInputRecord<RECORD>" %[[Bar2Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %output
// O3:   %[[Bar2Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %input, align 4, !noalias !17
// O3:   store %"struct.DispatchNodeInputRecord<RECORD>" %[[Bar2Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %output, align 4
// Od: %[[Bar2Ld:.+]] = load %"struct.DispatchNodeInputRecord<RECORD>", %"struct.DispatchNodeInputRecord<RECORD>"* %input, !noalias !18
// Od: store %"struct.DispatchNodeInputRecord<RECORD>" %[[Bar2Ld]], %"struct.DispatchNodeInputRecord<RECORD>"* %output
  output = foo2(input);
}

