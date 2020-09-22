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
// CHECK: [[foo:%\d+]] = OpString "foo"
// CHECK: [[c_name:%\d+]] = OpString "c"
// CHECK: [[b_name:%\d+]] = OpString "b"
// CHECK: [[a_name:%\d+]] = OpString "a"
// CHECK: [[func1:%\d+]] = OpString "foo.func1"
// CHECK: [[func0:%\d+]] = OpString "foo.func0"

// CHECK: [[parent:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[foo]] Structure {{%\d+}} 3 1 {{%\d+}} {{%\d+}} %uint_192 FlagIsProtected|FlagIsPrivate [[a:%\d+]] [[f0:%\d+]] [[b:%\d+]] [[f1:%\d+]] [[c:%\d+]]
// CHECK: [[c]] = OpExtInst %void [[set]] DebugTypeMember [[c_name]]
// CHECK: [[b]] = OpExtInst %void [[set]] DebugTypeMember [[b_name]]
// CHECK: [[a]] = OpExtInst %void [[set]] DebugTypeMember [[a_name]]
// CHECK: [[none:%\d+]] = OpExtInst %void [[set]] DebugInfoNone
// CHECK: [[f1]] = OpExtInst %void [[set]] DebugFunction [[func1]] {{%\d+}} {{%\d+}} 12 3 [[parent]] {{%\d+}} FlagIsProtected|FlagIsPrivate 12 [[none]]
// CHECK: [[f0]] = OpExtInst %void [[set]] DebugFunction [[func0]] {{%\d+}} {{%\d+}} 6 3 [[parent]] {{%\d+}} FlagIsProtected|FlagIsPrivate 6 %foo_func0

float4 main(float4 color : COLOR) : SV_TARGET {
  foo a;
  a.func0(1);

  return color;
}
