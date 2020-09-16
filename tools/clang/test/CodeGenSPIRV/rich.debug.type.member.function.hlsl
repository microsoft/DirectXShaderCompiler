// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

struct base {
  int p0;

  void f0(float arg) {
    p0 = arg;
  }

  float4 p1;

  int f1(int arg0, float arg1, bool arg2) {
    p0 = arg0;
    p1.y = arg1;
    if (arg2) return arg0;
    return p1.z;
  }

  bool p2;
};

struct foo : base {
  int a;

  void func0(float arg) {
    b.x = arg;
  }

  float4 b;

  int func1(int arg0, float arg1, bool arg2) {
    a = arg0;
    b.y = arg1;
    if (arg2) return arg0;
    return b.z;
  }

  bool c;
};

// CHECK: [[set:%\d+]] = OpExtInstImport "OpenCL.DebugInfo.100"

// CHECK: [[fooName:%\d+]] = OpString "foo"
// CHECK: [[bool:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Boolean
// CHECK: [[foo:%\d+]] = OpExtInst %void %1 DebugTypeComposite [[fooName]] Structure {{%\d+}} 22 8 {{%\d+}} [[fooName]] %uint_192 FlagIsProtected|FlagIsPrivate
// CHECK: [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float
// CHECK: [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Signed

// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate [[int]] [[foo]] [[int]] [[float]] [[bool]]
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate %void [[foo]] [[float]]

float4 main(float4 color : COLOR) : SV_TARGET {
  foo a;
  a.func0(1);
  a.func1(1, 1, 1);

  return color;
}
