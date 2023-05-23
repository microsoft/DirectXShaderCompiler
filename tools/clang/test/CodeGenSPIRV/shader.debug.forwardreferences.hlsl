// RUN: %dxc -T ps_6_0 -E main -fspv-debug=vulkan

// CHECK:        [[objectName:%\d+]] = OpString "Object"
// CHECK:      [[functionName:%\d+]] = OpString "Object.method"
// CHECK:            [[source:%\d+]] = OpExtInst %void %1 DebugSource {{%\d+}}
// CHECK:              [[unit:%\d+]] = OpExtInst %void %1 DebugCompilationUnit {{%\w+}} {{%\w+}} [[source]] {{%\w+}}
// CHECK:         [[composite:%\d+]] = OpExtInst %void %1 DebugTypeComposite [[objectName]] %uint_1 [[source]] {{%\w+}} {{%\w+}} [[unit]] [[objectName]] %uint_0 %uint_3 [[debugFunction:%\d+]]
// CHECK: [[debugTypeFunction:%\d+]] = OpExtInst %void %1 DebugTypeFunction {{%\w+}} %void [[composite]]
// CHECK:          [[debugFunction]] = OpExtInst %void %1 DebugFunction [[functionName]] [[debugTypeFunction]] [[source]] {{%\w+}} {{%\w+}} [[composite]] {{%\d+}} {{%\w+}} {{%\w+}}

struct Object
{
    void method() { }
};

float4 main(float4 color : COLOR) : SV_TARGET
{
    Object o;
    o.method();
    return float(0.f).xxxx;
}
