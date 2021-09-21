// RUN: %dxc -E main -T ps_6_0 -enable-operator-overloading %s | FileCheck %s

// CHECK: [[pos_y:%[^ ]+]] = call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
// CHECK: [[b:%[^ ]+]] = fptosi float [[pos_y]] to i32
// CHECK: call void @dx.op.storeOutput.i32(i32 5, i32 0, i32 0, i8 0, i32 [[b]])

struct Number {
    int n;

    void operator=(Number x) {
        n = x.n;
    }
};

int main(float4 pos: SV_Position) : SV_Target {
    Number a = {pos.x};
    Number b = {pos.y};
    a = b;
    return a.n;
}
