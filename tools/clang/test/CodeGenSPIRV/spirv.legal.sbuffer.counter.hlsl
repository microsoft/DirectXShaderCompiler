// Run: %dxc -T ps_6_0 -E main

struct S1 {
    float4 f;
};

struct S2 {
    float3 f;
};

struct S3 {
    float2 f;
};

RWStructuredBuffer<S1>      selectRWSBuffer(RWStructuredBuffer<S1>    paramRWSBuffer, bool selector);
AppendStructuredBuffer<S2>  selectASBuffer(AppendStructuredBuffer<S2>  paramASBuffer,  bool selector);
ConsumeStructuredBuffer<S3> selectCSBuffer(ConsumeStructuredBuffer<S3> paramCSBuffer,  bool selector);

// CHECK: %counter_var_globalRWSBuffer = OpVariable %_ptr_Uniform_type_ACSBuffer_counter Uniform
RWStructuredBuffer<S1>      globalRWSBuffer;
// CHECK: %counter_var_globalASBuffer = OpVariable %_ptr_Uniform_type_ACSBuffer_counter Uniform
AppendStructuredBuffer<S2>  globalASBuffer;
// CHECK: %counter_var_globalCSBuffer = OpVariable %_ptr_Uniform_type_ACSBuffer_counter Uniform
ConsumeStructuredBuffer<S3> globalCSBuffer;

// Counter variables for global static variables have an extra level of pointer.
// CHECK: %counter_var_staticgRWSBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
static RWStructuredBuffer<S1>      staticgRWSBuffer = globalRWSBuffer;
// CHECK: %counter_var_staticgASBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
static AppendStructuredBuffer<S2>  staticgASBuffer  = globalASBuffer;
// CHECK: %counter_var_staticgCSBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
static ConsumeStructuredBuffer<S3> staticgCSBuffer  = globalCSBuffer;

// Counter variables for function returns, function parameters, and local variables have an extra level of pointer.
// CHECK:      %counter_var_paramRWSBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_selectRWSBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_localRWSBufferMain = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_paramCSBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_selectCSBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_localASBufferMain = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_paramASBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_selectASBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_localRWSBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_localCSBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK-NEXT: %counter_var_localASBuffer = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// Counter variables for global static variables are initialized.
// CHECK: %main = OpFunction
// CHECK: OpStore %counter_var_staticgRWSBuffer %counter_var_globalRWSBuffer
// CHECK: OpStore %counter_var_staticgASBuffer %counter_var_globalASBuffer
// CHECK: OpStore %counter_var_staticgCSBuffer %counter_var_globalCSBuffer

// CHECK: %src_main = OpFunction
float4 main() : SV_Target {
// Update the counter variable associated with the parameter
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticgRWSBuffer
// CHECK-NEXT:                OpStore %counter_var_paramRWSBuffer [[ptr]]
    selectRWSBuffer(staticgRWSBuffer, true)
// Use the counter variable associated with the function
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_selectRWSBuffer
// CHECK-NEXT:     {{%\d+}} = OpAccessChain %_ptr_Uniform_int [[ptr]] %uint_0
        .IncrementCounter();

// Update the counter variable associated with the parameter
// CHECK:                     OpStore %counter_var_paramRWSBuffer %counter_var_globalRWSBuffer
// Update the counter variable associated with the lhs of the assignment
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_selectRWSBuffer
// CHECK-NEXT:                OpStore %counter_var_localRWSBufferMain [[ptr]]
    RWStructuredBuffer<S1> localRWSBufferMain = selectRWSBuffer(globalRWSBuffer, true);

// Use the counter variable associated with the local variable
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localRWSBufferMain
// CHECK-NEXT:     {{%\d+}} = OpAccessChain %_ptr_Uniform_int [[ptr]] %uint_0
    localRWSBufferMain.DecrementCounter();

// Update the counter variable associated with the parameter
// CHECK:                      OpStore %counter_var_paramCSBuffer %counter_var_globalCSBuffer
// CHECK:      [[call:%\d+]] = OpFunctionCall %_ptr_Uniform_type_ConsumeStructuredBuffer_S3 %selectCSBuffer
    S3 val3 = selectCSBuffer(globalCSBuffer, true)
// CHECK-NEXT: [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_selectCSBuffer
// CHECK-NEXT: [[ptr2:%\d+]] = OpAccessChain %_ptr_Uniform_int [[ptr1]] %uint_0
// CHECK-NEXT: [[prev:%\d+]] = OpAtomicISub %int [[ptr2]] %uint_1 %uint_0 %int_1
// CHECK-NEXT:  [[idx:%\d+]] = OpISub %int [[prev]] %int_1
// CHECK-NEXT: [[ptr3:%\d+]] = OpAccessChain %_ptr_Uniform_S3 [[call]] %uint_0 [[idx]]
// CHECK-NEXT:  [[val:%\d+]] = OpLoad %S3 [[ptr3]]
// CHECK-NEXT:  [[vec:%\d+]] = OpCompositeExtract %v2float [[val]] 0
// CHECK-NEXT: [[ptr4:%\d+]] = OpAccessChain %_ptr_Function_v2float %val3 %uint_0
// CHECK-NEXT:                 OpStore [[ptr4]] [[vec]]
        .Consume();

    float3 vec = float3(val3.f, 1.0);
    S2 val2 = {vec};

// Update the counter variable associated with the parameter
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticgASBuffer
// CHECK-NEXT:                OpStore %counter_var_paramASBuffer [[ptr]]

// CHECK:     [[call:%\d+]] = OpFunctionCall %_ptr_Uniform_type_AppendStructuredBuffer_S2 %selectASBuffer %param_var_paramASBuffer %param_var_selector_2
// CHECK-NEXT:                OpStore %localASBufferMain [[call]]
    AppendStructuredBuffer<S2> localASBufferMain = selectASBuffer(staticgASBuffer, false);
// Use the counter variable associated with the local variable
// CHECK-NEXT: [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_selectASBuffer
// CHECK-NEXT:                OpStore %counter_var_localASBufferMain [[ptr]]

// CHECK-NEXT: [[ptr1:%\d+]] = OpLoad %_ptr_Uniform_type_AppendStructuredBuffer_S2 %localASBufferMain
// CHECK-NEXT: [[ptr2:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localASBufferMain
// CHECK-NEXT: [[ptr3:%\d+]] = OpAccessChain %_ptr_Uniform_int [[ptr2]] %uint_0
// CHECK-NEXT:  [[idx:%\d+]] = OpAtomicIAdd %int [[ptr3]] %uint_1 %uint_0 %int_1
// CHECK-NEXT: [[ptr4:%\d+]] = OpAccessChain %_ptr_Uniform_S2 [[ptr1]] %uint_0 [[idx]]
// CHECK-NEXT:  [[val:%\d+]] = OpLoad %S2_0 %val2
// CHECK-NEXT:  [[vec:%\d+]] = OpCompositeExtract %v3float [[val]] 0
// CHECK-NEXT: [[ptr5:%\d+]] = OpAccessChain %_ptr_Uniform_v3float [[ptr4]] %uint_0
// CHECK-NEXT:                 OpStore [[ptr5]] [[vec]]
    localASBufferMain.Append(val2);

    return float4(val2, 2.0);
}

RWStructuredBuffer<S1>      selectRWSBuffer(RWStructuredBuffer<S1>    paramRWSBuffer, bool selector) {
// CHECK: OpStore %counter_var_localRWSBuffer %counter_var_globalRWSBuffer
    RWStructuredBuffer<S1>      localRWSBuffer = globalRWSBuffer;
    if (selector)
// Use the counter variable associated with the function
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_paramRWSBuffer
// CHECK-NEXT:                OpStore %counter_var_selectRWSBuffer [[ptr]]
// CHECK:                     OpReturnValue
        return paramRWSBuffer;
    else
// Use the counter variable associated with the function
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localRWSBuffer
// CHECK-NEXT:                OpStore %counter_var_selectRWSBuffer [[ptr]]
// CHECK:                     OpReturnValue
        return localRWSBuffer;
}

ConsumeStructuredBuffer<S3> selectCSBuffer(ConsumeStructuredBuffer<S3> paramCSBuffer,  bool selector) {
// CHECK: OpStore %counter_var_localCSBuffer %counter_var_globalCSBuffer
    ConsumeStructuredBuffer<S3> localCSBuffer  = globalCSBuffer;
    if (selector)
// Use the counter variable associated with the function
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_paramCSBuffer
// CHECK-NEXT:                OpStore %counter_var_selectCSBuffer [[ptr]]
// CHECK:                     OpReturnValue
        return paramCSBuffer;
    else
// Use the counter variable associated with the function
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localCSBuffer
// CHECK-NEXT:                OpStore %counter_var_selectCSBuffer [[ptr]]
// CHECK:                     OpReturnValue
        return localCSBuffer;
}

AppendStructuredBuffer<S2>  selectASBuffer(AppendStructuredBuffer<S2>  paramASBuffer,  bool selector) {
// CHECK: OpStore %counter_var_localASBuffer %counter_var_globalASBuffer
    AppendStructuredBuffer<S2>  localASBuffer  = globalASBuffer;
    if (selector)
// Use the counter variable associated with the function
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_paramASBuffer
// CHECK-NEXT:                OpStore %counter_var_selectASBuffer [[ptr]]
// CHECK:                     OpReturnValue
        return paramASBuffer;
    else
// Use the counter variable associated with the function
// CHECK:      [[ptr:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localASBuffer
// CHECK-NEXT:                OpStore %counter_var_selectASBuffer [[ptr]]
// CHECK:                     OpReturnValue
        return localASBuffer;
}
