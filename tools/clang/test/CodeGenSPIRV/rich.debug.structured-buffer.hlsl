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
// CHECK: [[temp1:%\d+]] = OpString "RWStructuredBuffer.TemplateParam"
// CHECK: [[RW_T:%\d+]] = OpString "@type.RWStructuredBuffer.T"
// CHECK: [[RW_S:%\d+]] = OpString "@type.RWStructuredBuffer.S"
// CHECK: [[temp0:%\d+]] = OpString "StructuredBuffer.TemplateParam"
// CHECK: [[SB_T:%\d+]] = OpString "@type.StructuredBuffer.T"
// CHECK: [[SB_S:%\d+]] = OpString "@type.StructuredBuffer.S"
// CHECK: [[inputBuffer:%\d+]] = OpString "inputBuffer"

// CHECK: [[T_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[T]] Structure
// CHECK: [[S_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[S]] Structure

// CHECK: [[param3:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplateParameter [[temp1]] [[T_ty]]
// CHECK: [[none:%\d+]] = OpExtInst %void [[set]] DebugInfoNone
// CHECK: [[RW_T_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[RW_T]] Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[none]]
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeTemplate [[RW_T_ty]] [[param3]]

// CHECK: [[param2:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplateParameter [[temp1]] [[S_ty]]
// CHECK: [[RW_S_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[RW_S]] Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[none]]
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeTemplate [[RW_S_ty]] [[param2]]

// CHECK: [[param1:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplateParameter [[temp0]] [[T_ty]]
// CHECK: [[SB_T_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[SB_T]] Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[none]]
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeTemplate [[SB_T_ty]] [[param1]]

// CHECK: [[param0:%\d+]] = OpExtInst %void [[set]] DebugTypeTemplateParameter [[temp0]] [[S_ty]]
// CHECK: [[SB_S_ty:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[SB_S]] Class {{%\d+}} 0 0 {{%\d+}} {{%\d+}} [[none]]
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeTemplate [[SB_S_ty]] [[param0]]

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[inputBuffer]] [[RW_S_ty]]

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable {{%\d+}} [[RW_T_ty]] {{%\d+}} {{\d+}} {{\d+}} {{%\d+}} {{%\d+}} %mySBuffer4
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable {{%\d+}} [[RW_S_ty]] {{%\d+}} {{\d+}} {{\d+}} {{%\d+}} {{%\d+}} %mySBuffer3
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable {{%\d+}} [[SB_T_ty]] {{%\d+}} {{\d+}} {{\d+}} {{%\d+}} {{%\d+}} %mySBuffer2
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugGlobalVariable {{%\d+}} [[SB_S_ty]] {{%\d+}} {{\d+}} {{\d+}} {{%\d+}} {{%\d+}} %mySBuffer1

void foo(RWStructuredBuffer<S> inputBuffer) {
  inputBuffer[0].a = 0;
}

float4 main() : SV_Target {
    foo(mySBuffer3);
    return 1.0;
}
