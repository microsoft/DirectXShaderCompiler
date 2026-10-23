// RUN: %dxc -E main -T cs_6_6 -Zi %s | FileCheck %s

// Check that the debug info created for a localized global variable in an
// inlined scope carries the inlinedAt chain back to the entry point, instead of
// pointing at the inlined subprogram from inside main, and that every inlined
// instance keeps its own value.

// CHECK: @main

// The sampler handle is annotated at the location of one of the declares of
// the localized kSampler, all of which are inlined into main.
// CHECK: call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.+}}, %dx.types.ResourceProperties { i32 14, i32 0 }), !dbg ![[annot_loc:[0-9]+]]

// Both inlined instances of helper share one variable, but each gets a value.
// CHECK: call void @llvm.dbg.value(metadata i32 %{{.+}}, i64 0, metadata ![[helper_var:[0-9]+]], metadata !{{[0-9]+}}), !dbg ![[helper_loc0:[0-9]+]] ; var:"global.g_index" !DIExpression() func:"helper"
// CHECK: call void @llvm.dbg.value(metadata i32 %{{.+}}, i64 0, metadata ![[helper_var]], metadata !{{[0-9]+}}), !dbg ![[helper_loc1:[0-9]+]] ; var:"global.g_index" !DIExpression() func:"helper"

// CHECK-DAG: ![[main_scope:[0-9]+]] = !DISubprogram(name: "main"
// CHECK-DAG: ![[helper_scope:[0-9]+]] = !DISubprogram(name: "helper"

// CHECK-DAG: ![[helper_var]] = !DILocalVariable(tag: DW_TAG_arg_variable, name: "global.g_index", arg: 0, scope: ![[helper_scope]],
// CHECK-DAG: ![[helper_loc0]] = !DILocation(line: [[@LINE+8]], scope: ![[helper_scope]], inlinedAt: ![[call_loc0:[0-9]+]])
// CHECK-DAG: ![[helper_loc1]] = !DILocation(line: [[@LINE+7]], scope: ![[helper_scope]], inlinedAt: ![[call_loc1:[0-9]+]])
// CHECK-DAG: ![[annot_loc]] = !DILocation(line: [[@LINE+5]], scope: !{{[0-9]+}}, inlinedAt: !{{[0-9]+}})
// CHECK-DAG: ![[call_loc0]] = distinct !DILocation(line: [[@LINE+15]], column: {{[0-9]+}}, scope: ![[main_scope]])
// CHECK-DAG: ![[call_loc1]] = distinct !DILocation(line: [[@LINE+14]], column: {{[0-9]+}}, scope: ![[main_scope]])

SamplerState getSampler() { return SamplerDescriptorHeap[6]; }
static const SamplerState kSampler = getSampler();
static uint g_index;

float4 helper(float uv) {
  Texture2D<float4> tex = ResourceDescriptorHeap[g_index];
  return tex.SampleLevel(kSampler, uv, 0);
}

[numthreads(1, 1, 1)] void main(uint id : SV_GroupIndex) {
  g_index = id;
  RWBuffer<float4> output = ResourceDescriptorHeap[2];
  output[0] = helper(0.5) + helper(0.25);
}
