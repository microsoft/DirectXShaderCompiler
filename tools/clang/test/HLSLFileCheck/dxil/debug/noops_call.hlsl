// RUN: %dxc -E main -T ps_6_0 %s -Od | FileCheck %s

typedef float4 MyCoolFloat4; 
static float4 myStaticGlobalVar = float4(1.0, 1.0, 1.0, 1.0);

// Local var with same name as outer scope
float4 localScopeVar_func(float4 val)
{
    float4 color = val * val;
    return color;
}

// Local var with same name as register
float4 localRegVar_func(float4 val)
{
    float4 r1 = val;
    return r1;
}

// Array
float4 array_func(float4 val)
{
    float result[4];
    result[0] = val.x;
    result[1] = val.y;
    result[2] = val.z;
    result[3] = val.w;
    return float4(result[0], result[1], result[2], result[3]);
}

// Typedef
float4 typedef_func(float4 val)
{
    MyCoolFloat4 result = val;
    return result;
}

// Global
float4 global_func(float4 val)
{
    myStaticGlobalVar *= val;
    return myStaticGlobalVar;
}

float4 depth4(float4 val)
{
    val = val * val;
    return val;
}

float4 depth3(float4 val)
{
    val = depth4(val) * val;
    return val;
}

float4 depth2(float4 val)
{
    val = depth3(val) * val;
    return val;
}

// CHECK: @dx.nothing = internal constant i32 0

[RootSignature("")]
float4 main( float4 unused : SV_POSITION, float4 color : COLOR ) : SV_Target
{
    // CHECK: %[[preserve_i32:[0-9]+]] = load i32, i32* @dx.preserve.value
    // CHECK: %[[preserve_f32:[0-9]+]] = sitofp i32 %[[preserve_i32]]
    float4 ret1 = localScopeVar_func(color);
    // ** call **
    // CHECK: load i32, i32* @dx.nothing
    // CHECK: %[[v1:.+]] = fmul
    // CHECK: %[[v2:.+]] = fmul
    // CHECK: %[[v3:.+]] = fmul
    // CHECK: %[[v4:.+]] = fmul
    // CHECK: fadd float %[[v1]], %[[preserve_f32]]
    // CHECK: fadd float %[[v2]], %[[preserve_f32]]
    // CHECK: fadd float %[[v3]], %[[preserve_f32]]
    // CHECK: fadd float %[[v4]], %[[preserve_f32]]
    // ** return **

    float4 ret2 = localRegVar_func(ret1);
    // ** call **
    // CHECK: load i32, i32* @dx.nothing
    // ** copy **
    // CHECK: fadd float %{{.+}}, %[[preserve_f32]]
    // CHECK: fadd float %{{.+}}, %[[preserve_f32]]
    // CHECK: fadd float %{{.+}}, %[[preserve_f32]]
    // CHECK: fadd float %{{.+}}, %[[preserve_f32]]
    // ** return **

    float4 ret3 = array_func(ret2);
    // ** call **
    // CHECK: load i32, i32* @dx.nothing
    // CHECK: store
    // CHECK: store
    // CHECK: store
    // CHECK: store
    // CHECK: load
    // CHECK: load
    // CHECK: load
    // CHECK: load
    // ** return **

    float4 ret4 = typedef_func(ret3);
    // ** call **
    // CHECK: load i32, i32* @dx.nothing
    // ** copy **
    // CHECK: fadd float %{{.+}}, %[[preserve_f32]]
    // CHECK: fadd float %{{.+}}, %[[preserve_f32]]
    // CHECK: fadd float %{{.+}}, %[[preserve_f32]]
    // CHECK: fadd float %{{.+}}, %[[preserve_f32]]
    // ** return **

    float4 ret5 = global_func(ret4);
    // ** call **
    // CHECK: load i32, i32* @dx.nothing
    // CHECK: %[[a1:.+]] = fmul
    // CHECK: %[[a2:.+]] = fmul
    // CHECK: %[[a3:.+]] = fmul
    // CHECK: %[[a4:.+]] = fmul
    // CHECK: fadd float %[[a1]], %[[preserve_f32]]
    // CHECK: fadd float %[[a2]], %[[preserve_f32]]
    // CHECK: fadd float %[[a3]], %[[preserve_f32]]
    // CHECK: fadd float %[[a4]], %[[preserve_f32]]
    // ** return **

    float4 ret6 = depth2(ret5);
    // ** call **
    // CHECK: load i32, i32* @dx.nothing
    // depth2() {
      // ** call **
      // CHECK: load i32, i32* @dx.nothing
      // depth3() {
        // ** call **
        // CHECK: load i32, i32* @dx.nothing
        // depth4() {
          // CHECK: %[[b1:.+]] = fmul
          // CHECK: %[[b2:.+]] = fmul
          // CHECK: %[[b3:.+]] = fmul
          // CHECK: %[[b4:.+]] = fmul
          // CHECK: fadd float %[[b1]], %[[preserve_f32]]
          // CHECK: fadd float %[[b2]], %[[preserve_f32]]
          // CHECK: fadd float %[[b3]], %[[preserve_f32]]
          // CHECK: fadd float %[[b4]], %[[preserve_f32]]
        // }
        // CHECK: %[[c1:.+]] = fmul
        // CHECK: %[[c2:.+]] = fmul
        // CHECK: %[[c3:.+]] = fmul
        // CHECK: %[[c4:.+]] = fmul
        // CHECK: fadd float %[[c1]], %[[preserve_f32]]
        // CHECK: fadd float %[[c2]], %[[preserve_f32]]
        // CHECK: fadd float %[[c3]], %[[preserve_f32]]
        // CHECK: fadd float %[[c4]], %[[preserve_f32]]
      // }
      // CHECK: %[[d1:.+]] = fmul
      // CHECK: %[[d2:.+]] = fmul
      // CHECK: %[[d3:.+]] = fmul
      // CHECK: %[[d4:.+]] = fmul
      // CHECK: fadd float %[[d1]], %[[preserve_f32]]
      // CHECK: fadd float %[[d2]], %[[preserve_f32]]
      // CHECK: fadd float %[[d3]], %[[preserve_f32]]
      // CHECK: fadd float %[[d4]], %[[preserve_f32]]
    // }

    return max(ret6, color);
    // CHECK: call float @dx.op.binary.f32(i32 35
    // CHECK: call float @dx.op.binary.f32(i32 35
    // CHECK: call float @dx.op.binary.f32(i32 35
    // CHECK: call float @dx.op.binary.f32(i32 35

}

