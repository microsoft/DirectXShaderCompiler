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
// CHECK: [[bool_name:%\d+]] = OpString "bool"
// CHECK: [[foo:%\d+]] = OpString "foo"
// CHECK: [[c_name:%\d+]] = OpString "c"
// CHECK: [[float_name:%\d+]] = OpString "float"
// CHECK: [[b_name:%\d+]] = OpString "b"
// CHECK: [[int_name:%\d+]] = OpString "int"
// CHECK: [[a_name:%\d+]] = OpString "a"
// CHECK: [[func1:%\d+]] = OpString "foo.func1"
// CHECK: [[func0:%\d+]] = OpString "foo.func0"

// CHECK: [[bool:%\d+]] = OpExtInst %void %1 DebugTypeBasic [[bool_name]] %uint_32 Boolean
// CHECK: [[parent:%\d+]] = OpExtInst %void [[set]] DebugTypeComposite [[foo]] Structure {{%\d+}} 3 8 {{%\d+}} {{%\d+}} %uint_192 FlagIsProtected|FlagIsPrivate [[a:%\d+]] [[b:%\d+]] [[c:%\d+]] [[f0:%\d+]] [[f1:%\d+]]

// CHECK: [[c]] = OpExtInst %void [[set]] DebugTypeMember [[c_name]] [[bool]] {{%\d+}} 19 8 [[parent]] %uint_160 %uint_32 FlagIsProtected|FlagIsPrivate
// CHECK: [[float:%\d+]] = OpExtInst %void %1 DebugTypeBasic [[float_name]] %uint_32 Float
// CHECK: [[v4f:%\d+]] = OpExtInst %void %1 DebugTypeVector [[float]] 4
// CHECK: [[b]] = OpExtInst %void [[set]] DebugTypeMember [[b_name]] [[v4f]] {{%\d+}} 10 10 [[parent]] %uint_32 %uint_128 FlagIsProtected|FlagIsPrivate
// CHECK: [[int:%\d+]] = OpExtInst %void %1 DebugTypeBasic [[int_name]] %uint_32 Signed
// CHECK: [[a]] = OpExtInst %void [[set]] DebugTypeMember [[a_name]] [[int]] {{%\d+}} 4 7 [[parent]] %uint_0 %uint_32 FlagIsProtected|FlagIsPrivate
// CHECK: [[f1]] = OpExtInst %void [[set]] DebugFunction [[func1]] {{%\d+}} {{%\d+}} 12 3 [[parent]] {{%\d+}} FlagIsProtected|FlagIsPrivate 12 %foo_func1
// CHECK: [[f0]] = OpExtInst %void [[set]] DebugFunction [[func0]] {{%\d+}} {{%\d+}} 6 3 [[parent]] {{%\d+}} FlagIsProtected|FlagIsPrivate 6 %foo_func0

float4 main(float4 color : COLOR) : SV_TARGET {
  foo a;
  a.func0(1);
  a.func1(1, 1, 1);

  return color;
}
