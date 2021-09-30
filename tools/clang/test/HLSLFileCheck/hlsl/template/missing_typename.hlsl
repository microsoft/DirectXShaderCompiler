// RUN: %dxc -T vs_6_0 -E VSMain -enable-templates -DTYPENAME= %s | FileCheck %s -check-prefix=MISSING
// RUN: %dxc -T vs_6_0 -E VSMain -enable-templates -DTYPENAME=typename %s | FileCheck %s -check-prefix=PRESENT
// RUN: %dxc -T vs_6_0 -E VSMain -HV 2021 -DTYPENAME= %s | FileCheck %s -check-prefix=MISSING
// RUN: %dxc -T vs_6_0 -E VSMain -HV 2021 -DTYPENAME=typename %s | FileCheck %s -check-prefix=PRESENT

// MISSING: error: missing 'typename' prior to dependent type name 'TVec::value_type'
// PRESENT: define void @VSMain() {

template <typename T>
struct Vec2 {
    T x, y;
    T Length() {
        return sqrt(x*x + y*y);
    }
    typedef T value_type;
};

template <typename TVec>
TYPENAME TVec::value_type Length(TVec v) { // <-- Must be "typename TVec::value_type"
    return v.Length();
}

void VSMain() {
    Vec2<int> v_i;
    Vec2<float> v_f;
   
    int   a = Length(v_i);
    float b = Length(v_f);
}
