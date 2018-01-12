// Run: %dxc -T vs_6_0 -E main

struct Bundle {
      RWStructuredBuffer<float> rw;
  AppendStructuredBuffer<float> append;
 ConsumeStructuredBuffer<float> consume;
};

struct TwoBundle {
    Bundle b1;
    Bundle b2;
};

struct Wrapper {
    TwoBundle b;
};

      RWStructuredBuffer<float> globalRWSBuffer;
  AppendStructuredBuffer<float> globalASBuffer;
 ConsumeStructuredBuffer<float> globalCSBuffer;

Bundle  CreateBundle();
Wrapper CreateWrapper();
Wrapper ReturnWrapper(Wrapper wrapper);

// Static variable
static Bundle  staticBundle  = CreateBundle();
static Wrapper staticWrapper = CreateWrapper();

// CHECK: %counter_var_staticBundle_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_staticBundle_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_staticBundle_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK: %counter_var_staticWrapper_0_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_staticWrapper_0_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_staticWrapper_0_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_staticWrapper_0_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_staticWrapper_0_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_staticWrapper_0_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK: %counter_var_CreateBundle_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_CreateBundle_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_CreateBundle_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK: %counter_var_CreateWrapper_0_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_CreateWrapper_0_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_CreateWrapper_0_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_CreateWrapper_0_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_CreateWrapper_0_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_CreateWrapper_0_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK: %counter_var_localWrapper_0_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_localWrapper_0_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_localWrapper_0_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_localWrapper_0_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_localWrapper_0_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_localWrapper_0_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK: %counter_var_wrapper_0_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_wrapper_0_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_wrapper_0_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_wrapper_0_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_wrapper_0_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_wrapper_0_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK: %counter_var_ReturnWrapper_0_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_ReturnWrapper_0_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_ReturnWrapper_0_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_ReturnWrapper_0_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_ReturnWrapper_0_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_ReturnWrapper_0_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK: %counter_var_b_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_b_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_b_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK: %counter_var_w_0_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_w_0_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_w_0_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_w_0_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_w_0_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_w_0_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// CHECK-LABEL: %main = OpFunction

    // Assign to static variable
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_CreateBundle_0
// CHECK-NEXT:                OpStore %counter_var_staticBundle_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_CreateBundle_1
// CHECK-NEXT:                OpStore %counter_var_staticBundle_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_CreateBundle_2
// CHECK-NEXT:                OpStore %counter_var_staticBundle_2 [[src]]

// CHECK-LABEL: %src_main = OpFunction
float main() : VALUE {
    // Assign to the parameter
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticWrapper_0_0_0
// CHECK-NEXT:                OpStore %counter_var_wrapper_0_0_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticWrapper_0_0_1
// CHECK-NEXT:                OpStore %counter_var_wrapper_0_0_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticWrapper_0_0_2
// CHECK-NEXT:                OpStore %counter_var_wrapper_0_0_2 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticWrapper_0_1_0
// CHECK-NEXT:                OpStore %counter_var_wrapper_0_1_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticWrapper_0_1_1
// CHECK-NEXT:                OpStore %counter_var_wrapper_0_1_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticWrapper_0_1_2
// CHECK-NEXT:                OpStore %counter_var_wrapper_0_1_2 [[src]]
    // Make the call
// CHECK:          {{%\d+}} = OpFunctionCall %Wrapper %ReturnWrapper %param_var_wrapper
    // Assign to the return value
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_ReturnWrapper_0_0_0
// CHECK-NEXT:                OpStore %counter_var_localWrapper_0_0_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_ReturnWrapper_0_0_1
// CHECK-NEXT:                OpStore %counter_var_localWrapper_0_0_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_ReturnWrapper_0_0_2
// CHECK-NEXT:                OpStore %counter_var_localWrapper_0_0_2 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_ReturnWrapper_0_1_0
// CHECK-NEXT:                OpStore %counter_var_localWrapper_0_1_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_ReturnWrapper_0_1_1
// CHECK-NEXT:                OpStore %counter_var_localWrapper_0_1_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_ReturnWrapper_0_1_2
// CHECK-NEXT:                OpStore %counter_var_localWrapper_0_1_2 [[src]]
    Wrapper localWrapper = ReturnWrapper(staticWrapper);

// CHECK:      [[cnt:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_0_0
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_int [[cnt]] %uint_0
// CHECK-NEXT:     {{%\d+}} = OpAtomicIAdd %int [[ptr]] %uint_1 %uint_0 %int_1
    localWrapper.b.b1.rw.IncrementCounter();
// CHECK:      [[cnt:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_1_1
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_int [[cnt]] %uint_0
// CHECK-NEXT: [[add:%\d+]] = OpAtomicIAdd %int [[ptr]] %uint_1 %uint_0 %int_1
// CHECK-NEXT:     {{%\d+}} = OpAccessChain %_ptr_Uniform_float {{%\d+}} %uint_0 [[add]]
    localWrapper.b.b2.append.Append(5.0);

// CHECK:      [[cnt:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_ReturnWrapper_0_0_2
// CHECK-NEXT: [[ptr:%\d+]] = OpAccessChain %_ptr_Uniform_int [[cnt]] %uint_0
// CHECK-NEXT: [[sub:%\d+]] = OpAtomicISub %int [[ptr]] %uint_1 %uint_0 %int_1
// CHECK-NEXT: [[pre:%\d+]] = OpISub %int [[sub]] %int_1
// CHECK-NEXT:     {{%\d+}} = OpAccessChain %_ptr_Uniform_float {{%\d+}} %uint_0 [[pre]]
    return ReturnWrapper(staticWrapper).b.b1.consume.Consume();
}

// CHECK-LABEL: %CreateBundle = OpFunction
Bundle CreateBundle() {
    Bundle b;
    // Assign to final struct fields who have associated counters
// CHECK: OpStore %counter_var_b_0 %counter_var_globalRWSBuffer
    b.rw      = globalRWSBuffer;
// CHECK: OpStore %counter_var_b_1 %counter_var_globalASBuffer
    b.append  = globalASBuffer;
// CHECK: OpStore %counter_var_b_2 %counter_var_globalCSBuffer
    b.consume = globalCSBuffer;

    // Assign from local variable
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_b_0
// CHECK-NEXT:                OpStore %counter_var_CreateBundle_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_b_1
// CHECK-NEXT:                OpStore %counter_var_CreateBundle_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_b_2
// CHECK-NEXT:                OpStore %counter_var_CreateBundle_2 [[src]]
    return b;
}

// CHECK-LABEL: %CreateWrapper = OpFunction
Wrapper CreateWrapper() {
    Wrapper w;

    // Assign from final struct fields who have associated counters
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticBundle_0
// CHECK-NEXT:                OpStore %counter_var_w_0_0_0 [[src]]
    w.b.b1.rw      = staticBundle.rw;
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticBundle_1
// CHECK-NEXT:                OpStore %counter_var_w_0_0_1 [[src]]
    w.b.b1.append  = staticBundle.append;
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticBundle_2
// CHECK-NEXT:                OpStore %counter_var_w_0_0_2 [[src]]
    w.b.b1.consume = staticBundle.consume;

    // Assign to intermediate structs whose fields have associated counters
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticBundle_0
// CHECK-NEXT:                OpStore %counter_var_w_0_1_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticBundle_1
// CHECK-NEXT:                OpStore %counter_var_w_0_1_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_staticBundle_2
// CHECK-NEXT:                OpStore %counter_var_w_0_1_2 [[src]]
    w.b.b2         = staticBundle;

    // Assign from intermediate structs whose fields have associated counters
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_w_0_0_0
// CHECK-NEXT:                OpStore %counter_var_staticBundle_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_w_0_0_1
// CHECK-NEXT:                OpStore %counter_var_staticBundle_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_w_0_0_2
// CHECK-NEXT:                OpStore %counter_var_staticBundle_2 [[src]]
    staticBundle   = w.b.b1;

// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_w_0_1_0
// CHECK-NEXT:                OpStore %counter_var_w_0_0_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_w_0_1_1
// CHECK-NEXT:                OpStore %counter_var_w_0_0_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_w_0_1_2
// CHECK-NEXT:                OpStore %counter_var_w_0_0_2 [[src]]
    w.b.b1         = w.b.b2;

    return w;
}

// CHECK-LABEL: %ReturnWrapper = OpFunction
Wrapper ReturnWrapper(Wrapper wrapper) {
    // Assign from parameter
// CHECK:      [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_wrapper_0_0_0
// CHECK-NEXT:                OpStore %counter_var_ReturnWrapper_0_0_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_wrapper_0_0_1
// CHECK-NEXT:                OpStore %counter_var_ReturnWrapper_0_0_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_wrapper_0_0_2
// CHECK-NEXT:                OpStore %counter_var_ReturnWrapper_0_0_2 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_wrapper_0_1_0
// CHECK-NEXT:                OpStore %counter_var_ReturnWrapper_0_1_0 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_wrapper_0_1_1
// CHECK-NEXT:                OpStore %counter_var_ReturnWrapper_0_1_1 [[src]]
// CHECK-NEXT: [[src:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_wrapper_0_1_2
// CHECK-NEXT:                OpStore %counter_var_ReturnWrapper_0_1_2 [[src]]
    return wrapper;
}
