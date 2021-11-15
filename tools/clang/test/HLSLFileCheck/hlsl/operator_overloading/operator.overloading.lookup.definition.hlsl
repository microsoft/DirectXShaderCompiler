// RUN: %dxc -E main -T ps_6_0 -HV 2021 %s | FileCheck %s

// CHECK: define void @main()

struct S {
    float a;

    void operator=(float x) {
        a = x;
    }

    float operator+(float x) {
        return a + x;
    }
};

struct Number {
    int n;

    void operator=(float x) {
        n = x;
    }
};

int main(float4 pos: SV_Position) : SV_Target {
    S s1;
    S s2;
    s1 = s2;
    s1 = 0.2;
    s1 = s1 + 0.1;

    Number a = {pos.x};
    Number b = {pos.y};
    a = pos.x;
    return a.n;
}
