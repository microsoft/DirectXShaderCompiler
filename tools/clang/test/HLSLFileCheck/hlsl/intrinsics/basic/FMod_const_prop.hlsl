// RUN: %dxc -T lib_6_x -fcgl %s -E main | %FileCheck %s

// Ensure fmod is constant propagated during codegen
// CHECK: call void {{.*}}results{{.*}}(float 2.500000e+00, float -2.500000e+00, float 2.500000e+00, float -2.500000e+00)
// CHECK: call void {{.*}}results{{.*}}(float 2.500000e+00, float -2.500000e+00, float 2.500000e+00, float -2.500000e+00)

void results(float a, float b, float c, float d);

void main() {
    results(
        fmod(5.5, 3.0),
        fmod(-5.5, 3.0),
        fmod(5.5, -3.0),
        fmod(-5.5, -3.0));
    results(
        fmod(5.5f, 3.0f),
        fmod(-5.5f, 3.0f),
        fmod(5.5f, -3.0f),
        fmod(-5.5f, -3.0f));
}
