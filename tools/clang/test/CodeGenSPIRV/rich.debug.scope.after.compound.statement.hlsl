// Run: %dxc -T vs_6_0 -E main -fspv-debug=rich

struct VS_OUTPUT {
  float4 pos : SV_POSITION;
};

//CHECK: [[fn:%\d+]] = OpExtInst %void [[ext:%\d+]] DebugFunction
//CHECK: [[bb0:%\d+]] = OpExtInst %void [[ext]] DebugLexicalBlock {{%\d+}} 14 {{\d+}} [[fn]]
//CHECK: [[bb1:%\d+]] = OpExtInst %void [[ext]] DebugLexicalBlock {{%\d+}} 21 {{\d+}} [[bb0]]
//CHECK: [[bb2:%\d+]] = OpExtInst %void [[ext]] DebugLexicalBlock {{%\d+}} 27 {{\d+}} [[bb1]]
//CHECK: [[a:%\d+]] = OpExtInst %void [[ext]] DebugLocalVariable {{%\d+}} {{%\d+}} {{%\d+}} 32 {{\d+}} [[bb2]]

VS_OUTPUT main(float4 pos : POSITION,
               float4 color : COLOR) {
//CHECK: OpLabel
//CHECK: DebugScope [[bb0]]
  float a = 1.0;
  float b = 2.0;
  float c = 3.0;
  float x = a + b + c;
  {
//CHECK:      DebugScope [[bb1]]
//CHECK-NEXT: OpLine [[file:%\d+]] 24
    float a = 3.0;
    float b = 4.0;
    x += a + b + c;
    {
//CHECK:      DebugScope [[bb2]]
//CHECK-NEXT: OpLine [[file:%\d+]] 32
//CHECK-NEXT: OpStore [[var_a:%\w+]] %float_6
//CHECK:      DebugDeclare [[a]] [[var_a]]
      float a = 6.0;
      x += a + b + c;
    }
//CHECK:      DebugScope [[bb1]]
//CHECK-NEXT: OpLine [[file:%\d+]] 37
    x += a + b + c;
  }
//CHECK:      DebugScope [[bb0]]
//CHECK-NEXT: OpLine [[file:%\d+]] 41
  x += a + b + c;

  VS_OUTPUT vout;
  vout.pos = pos;
  vout.pos.w += x * 0.000001;
  return vout;
}
