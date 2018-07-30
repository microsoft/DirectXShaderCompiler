// Run: %dxc -T vs_6_0 -E main

struct Bundle {
      RWStructuredBuffer<float> rw;
  AppendStructuredBuffer<float> append;
 ConsumeStructuredBuffer<float> consume;
};

// Counter variables for the this object of getCSBuffer()
// CHECK: %counter_var_getCSBuffer_this_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getCSBuffer_this_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getCSBuffer_this_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getCSBuffer_this_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getCSBuffer_this_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getCSBuffer_this_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// Counter variables for the this object of getASBuffer()
// CHECK: %counter_var_getASBuffer_this_0_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getASBuffer_this_0_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getASBuffer_this_0_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getASBuffer_this_0_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getASBuffer_this_0_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getASBuffer_this_0_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

// Counter variables for the this object of getRWSBuffer()
// CHECK: %counter_var_getRWSBuffer_this_0_0_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getRWSBuffer_this_0_0_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getRWSBuffer_this_0_0_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getRWSBuffer_this_0_1_0 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getRWSBuffer_this_0_1_1 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private
// CHECK: %counter_var_getRWSBuffer_this_0_1_2 = OpVariable %_ptr_Private__ptr_Uniform_type_ACSBuffer_counter Private

struct TwoBundle {
    Bundle b1;
    Bundle b2;

    // Checks at the end of the file
    ConsumeStructuredBuffer<float> getCSBuffer() { return b1.consume; }
};

struct Wrapper {
    TwoBundle b;

    // Checks at the end of the file
    AppendStructuredBuffer<float> getASBuffer() { return b.b1.append; }

    // Checks at the end of the file
    RWStructuredBuffer<float> getRWSBuffer() { return b.b2.rw; }
};

// CHECK-LABLE: %src_main = OpFunction
float main() : VVV {
    TwoBundle localBundle;
    Wrapper   localWrapper;

// CHECK:      [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localBundle_0_0
// CHECK-NEXT:                    OpStore %counter_var_getCSBuffer_this_0_0 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localBundle_0_1
// CHECK-NEXT:                    OpStore %counter_var_getCSBuffer_this_0_1 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localBundle_0_2
// CHECK-NEXT:                    OpStore %counter_var_getCSBuffer_this_0_2 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localBundle_1_0
// CHECK-NEXT:                    OpStore %counter_var_getCSBuffer_this_1_0 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localBundle_1_1
// CHECK-NEXT:                    OpStore %counter_var_getCSBuffer_this_1_1 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localBundle_1_2
// CHECK-NEXT:                    OpStore %counter_var_getCSBuffer_this_1_2 [[counter]]
// CHECK-NEXT:                    OpFunctionCall %_ptr_Uniform_type_ConsumeStructuredBuffer_float %TwoBundle_getCSBuffer %localBundle
    float value = localBundle.getCSBuffer().Consume();

// CHECK:      [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_0_0
// CHECK-NEXT:                    OpStore %counter_var_getASBuffer_this_0_0_0 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_0_1
// CHECK-NEXT:                    OpStore %counter_var_getASBuffer_this_0_0_1 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_0_2
// CHECK-NEXT:                    OpStore %counter_var_getASBuffer_this_0_0_2 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_1_0
// CHECK-NEXT:                    OpStore %counter_var_getASBuffer_this_0_1_0 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_1_1
// CHECK-NEXT:                    OpStore %counter_var_getASBuffer_this_0_1_1 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_1_2
// CHECK-NEXT:                    OpStore %counter_var_getASBuffer_this_0_1_2 [[counter]]
// CHECK-NEXT:                    OpFunctionCall %_ptr_Uniform_type_AppendStructuredBuffer_float %Wrapper_getASBuffer %localWrapper
    localWrapper.getASBuffer().Append(4.2);

// CHECK:      [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_0_0
// CHECK-NEXT:                    OpStore %counter_var_getRWSBuffer_this_0_0_0 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_0_1
// CHECK-NEXT:                    OpStore %counter_var_getRWSBuffer_this_0_0_1 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_0_2
// CHECK-NEXT:                    OpStore %counter_var_getRWSBuffer_this_0_0_2 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_1_0
// CHECK-NEXT:                    OpStore %counter_var_getRWSBuffer_this_0_1_0 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_1_1
// CHECK-NEXT:                    OpStore %counter_var_getRWSBuffer_this_0_1_1 [[counter]]
// CHECK-NEXT: [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_localWrapper_0_1_2
// CHECK-NEXT:                    OpStore %counter_var_getRWSBuffer_this_0_1_2 [[counter]]
// CHECK-NEXT:                    OpFunctionCall %_ptr_Uniform_type_RWStructuredBuffer_float %Wrapper_getRWSBuffer %localWrapper
    RWStructuredBuffer<float> localRWSBuffer = localWrapper.getRWSBuffer();

    return localRWSBuffer[5];
}

// CHECK-LABEL: %TwoBundle_getCSBuffer = OpFunction
// CHECK:             [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_getCSBuffer_this_0_2
// CHECK-NEXT:                           OpStore %counter_var_getCSBuffer [[counter]]

// CHECK-LABEL: %Wrapper_getASBuffer = OpFunction
// CHECK:           [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_getASBuffer_this_0_0_1
// CHECK-NEXT:                         OpStore %counter_var_getASBuffer [[counter]]

// CHECK-LABEL: %Wrapper_getRWSBuffer = OpFunction
// CHECK:            [[counter:%\d+]] = OpLoad %_ptr_Uniform_type_ACSBuffer_counter %counter_var_getRWSBuffer_this_0_1_0
// CHECK-NEXT:                          OpStore %counter_var_getRWSBuffer [[counter]]
