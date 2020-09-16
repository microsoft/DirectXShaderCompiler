// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

struct S {
    float  a;
    float3 b;
    int2 c;
};

struct T {
    float  a;
    float3 b;
    S      c;
    int2   d;
};

StructuredBuffer<S> mySBuffer1 : register(t1);
StructuredBuffer<T> mySBuffer2 : register(t2);

RWStructuredBuffer<S> mySBuffer3 : register(u3);
RWStructuredBuffer<T> mySBuffer4 : register(u4);

// CHECK: [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"
// CHECK: [[T:%\d+]] = OpString "T"
// CHECK: [[S:%\d+]] = OpString "S"
// CHECK: [[RW_S:%\d+]] = OpString "@type.RWStructuredBuffer.S"
// CHECK: [[param:%\d+]] = OpString "TemplateParam"
// CHECK: [[inputBuffer:%\d+]] = OpString "inputBuffer"
// CHECK: [[RW_T:%\d+]] = OpString "@type.RWStructuredBuffer.T"
// CHECK: [[SB_T:%\d+]] = OpString "@type.StructuredBuffer.T"
// CHECK: [[SB_S:%\d+]] = OpString "@type.StructuredBuffer.S"

// CHECK: [[T_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[T]] Structure
// CHECK: [[S_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[S]] Structure

// CHECK: [[none:%\d+]] = OpExtInst %void [[set]] DebugInfoNone
// CHECK: [[RW_S_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[RW_S]] Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[none]]
// CHECK: [[param2:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplateParameter [[param]] [[S_ty]]
// CHECK: [[RW_S_temp:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplate [[RW_S_ty]] [[param2]]

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[inputBuffer]] [[RW_S_temp]]

// CHECK: [[RW_T_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[RW_T]] Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[none]]
// CHECK: [[param3:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplateParameter [[param]] [[T_ty]]
// CHECK: [[RW_T_temp:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplate [[RW_T_ty]] [[param3]]

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable {{%\d+}} [[RW_T_temp]] {{%\d+}} {{\d+}} {{\d+}} {{%\d+}} {{%\d+}} %mySBuffer4
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable {{%\d+}} [[RW_S_temp]] {{%\d+}} {{\d+}} {{\d+}} {{%\d+}} {{%\d+}} %mySBuffer3

// CHECK: [[SB_T_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[SB_T]] Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[none]]
// CHECK: [[param1:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplateParameter [[param]] [[T_ty]]
// CHECK: [[SB_T_temp:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplate [[SB_T_ty]] [[param1]]

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable {{%\d+}} [[SB_T_temp]] {{%\d+}} {{\d+}} {{\d+}} {{%\d+}} {{%\d+}} %mySBuffer2

// CHECK: [[SB_S_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[SB_S]] Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[none]]
// CHECK: [[param0:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplateParameter [[param]] [[S_ty]]
// CHECK: [[SB_S_temp:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplate [[SB_S_ty]] [[param0]]

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable {{%\d+}} [[SB_S_temp]] {{%\d+}} {{\d+}} {{\d+}} {{%\d+}} {{%\d+}} %mySBuffer1

void foo(RWStructuredBuffer<S> inputBuffer) {
  inputBuffer[0].a = 0;
}

float4 main() : SV_Target {
    foo(mySBuffer3);
    return 1.0;
}
