// Run: %dxc -T ps_6_0 -E main -fspv-debug=rich

struct foo {
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
// CHECK: [[func1:%\d+]] = OpString "foo.func1"
// CHECK: [[arg2:%\d+]] = OpString "arg2"
// CHECK: [[arg1:%\d+]] = OpString "arg1"
// CHECK: [[arg0:%\d+]] = OpString "arg0"
// CHECK: [[this:%\d+]] = OpString "this"
// CHECK: [[func0:%\d+]] = OpString "foo.func0"
// CHECK: [[arg:%\d+]] = OpString "arg"

// CHECK: [[bool:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Boolean
// CHECK: [[foo:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[fooName]] Structure {{%\d+}} 3 8

// CHECK: [[float:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Float
// CHECK: [[int:%\d+]] = OpExtInst %void [[set]] DebugTypeBasic {{%\d+}} %uint_32 Signed

// CHECK: [[func1Type:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate [[int]] [[foo]] [[int]] [[float]] [[bool]]
// CHECK: [[f1:%\d+]] = OpExtInst %void [[set]] DebugFunction [[func1]] [[func1Type]] {{%\d+}} 12 3 [[foo]] {{%\d+}} FlagIsProtected|FlagIsPrivate 12 %foo_func1
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[arg2]] {{%\d+}} {{%\d+}} 12 40 [[f1]] FlagIsLocal 4
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[arg1]] {{%\d+}} {{%\d+}} 12 29 [[f1]] FlagIsLocal 3
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[arg0]] {{%\d+}} {{%\d+}} 12 17 [[f1]] FlagIsLocal 2
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[this]] [[foo]] {{%\d+}} 12 3 [[f1]] FlagArtificial|FlagObjectPointer 1
// CHECK: [[func0Type:%\d+]] = OpExtInst %void [[set]] DebugTypeFunction FlagIsProtected|FlagIsPrivate %void [[foo]] [[float]]
// CHECK: [[f0:%\d+]] = OpExtInst %void [[set]] DebugFunction [[func0]] [[func0Type]] {{%\d+}} 6 3 [[foo]] {{%\d+}} FlagIsProtected|FlagIsPrivate 6 %foo_func0
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[arg]] {{%\d+}} {{%\d+}} 6 20 [[f0]] FlagIsLocal 2
// CHECK: {{%\d+}} = OpExtInst %void [[set]] DebugLocalVariable [[this]] [[foo]] {{%\d+}} 6 3 [[f0]] FlagArtificial|FlagObjectPointer 1

float4 main(float4 color : COLOR) : SV_TARGET {
  foo a;
  a.func0(1);
  a.func1(1, 1, 1);

  return color;
}
