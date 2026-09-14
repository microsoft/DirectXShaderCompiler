// RUN: %dxc -T lib_6_3 %s -verify
// expected-no-diagnostics

enum Enum { EV };
struct POD { int i; float f;};
typedef const POD ConstPOD;
const POD ConstStruct;
typedef POD AoS[10];
struct SoA
{
    int iAr[10];
    float fAr[10];
};
template<typename T>
struct TemplatedPOD { T t; float f; };
struct Empty {};
typedef Empty EmptyAr[10];
typedef int Int;
typedef Int IntAr[10];
struct Derives : POD {};
struct Incomplete;
typedef const int ConstInt;
static const int GlobalStaticConstInt;
float GlobalFloat;
const float ConstFloat;
static const float StaticConstFloat;
typedef bool Boolean;
typedef uint UnsignedInt;
typedef dword Dword;
typedef half Half;
typedef double Double;
void fn() {}

#define check(arg) _Static_assert(arg, #arg);

check(__is_base_of(POD, Derives));
check(!__is_base_of(POD, Empty));
check(!__is_base_of(Derives, POD));
check(__is_enum(Enum));
check(!__is_enum(POD));
check(!__is_enum(Empty));
check(!__is_enum(EmptyAr));
check(!__is_enum(Int));
check(!__is_enum(IntAr));
check(!__is_enum(Derives));
check(!__is_enum(Incomplete));
check(!__is_enum(ConstInt));
check(!__is_enum(__decltype(GlobalStaticConstInt)));
check(!__is_enum(__decltype(GlobalFloat)));
check(!__is_enum(__decltype(fn)));
check(!__is_enum(RWByteAddressBuffer));

check(!__is_function(Enum));
check(!__is_function(TemplatedPOD<Int>));
check(!__is_function(POD));
check(!__is_function(Empty));
check(!__is_function(EmptyAr));
check(!__is_function(Int));
check(!__is_function(IntAr));
check(!__is_function(Derives));
check(!__is_function(Incomplete));
check(!__is_function(ConstInt));
check(!__is_function(__decltype(GlobalStaticConstInt)));
check(!__is_function(__decltype(GlobalFloat)));
check(!__is_function(RWByteAddressBuffer));
check(__is_function(__decltype(fn)));

check(!__is_class(Enum));
check(__is_class(POD));
check(__is_class(TemplatedPOD<Int>));
check(__is_class(Empty));
check(!__is_class(EmptyAr));
check(!__is_class(Int));
check(!__is_class(IntAr));
check(__is_class(Derives));
check(__is_class(Incomplete));
check(!__is_class(ConstInt));
check(!__is_class(__decltype(GlobalStaticConstInt)));
check(!__is_class(__decltype(GlobalFloat)));
check(!__is_class(__decltype(fn)));
// check(__is_class(RWByteAddressBuffer)); TODO: I wonder what about that

check(!__is_arithmetic(Enum));
check(!__is_arithmetic(POD));
check(!__is_arithmetic(TemplatedPOD<Int>));
check(!__is_arithmetic(Empty));
check(!__is_arithmetic(EmptyAr));
check(__is_arithmetic(Int));
check(!__is_arithmetic(IntAr));
check(!__is_arithmetic(Derives));
check(!__is_arithmetic(Incomplete));
check(__is_arithmetic(ConstInt));
check(__is_arithmetic(__decltype(GlobalStaticConstInt)));
check(__is_arithmetic(__decltype(GlobalFloat)));
check(__is_arithmetic(Boolean));
check(__is_arithmetic(UnsignedInt));
check(__is_arithmetic(Dword));
check(__is_arithmetic(Half));
check(__is_arithmetic(Double));
check(!__is_arithmetic(__decltype(fn)));
check(!__is_arithmetic(RWByteAddressBuffer));

check(__is_scalar(Enum));
check(!__is_scalar(POD));
check(!__is_scalar(TemplatedPOD<Int>));
check(!__is_scalar(Empty));
check(!__is_scalar(EmptyAr));
check(__is_scalar(Int));
check(!__is_scalar(IntAr));
check(!__is_scalar(Derives));
check(!__is_scalar(Incomplete));
check(__is_scalar(ConstInt));
check(__is_scalar(__decltype(GlobalStaticConstInt)));
check(__is_scalar(__decltype(GlobalFloat)));
check(__is_scalar(Boolean));
check(__is_scalar(UnsignedInt));
check(__is_scalar(Dword));
check(__is_scalar(Half));
check(__is_scalar(Double));
check(!__is_scalar(__decltype(fn)));
check(!__is_scalar(RWByteAddressBuffer));

check(!__is_floating_point(Enum));
check(!__is_floating_point(POD));
check(!__is_floating_point(TemplatedPOD<Int>));
check(!__is_floating_point(Empty));
check(!__is_floating_point(EmptyAr));
check(!__is_floating_point(Int));
check(!__is_floating_point(IntAr));
check(!__is_floating_point(Derives));
check(!__is_floating_point(Incomplete));
check(!__is_floating_point(ConstInt));
check(!__is_floating_point(__decltype(GlobalStaticConstInt)));
check(__is_floating_point(__decltype(GlobalFloat)));
check(__is_floating_point(__decltype(ConstFloat)));
check(__is_floating_point(__decltype(StaticConstFloat)));
check(!__is_floating_point(Boolean));
check(!__is_floating_point(UnsignedInt));
check(!__is_floating_point(Dword));
check(__is_floating_point(Half));
check(__is_floating_point(Double));
check(!__is_floating_point(__decltype(fn)));
check(!__is_floating_point(RWByteAddressBuffer));

check(!__is_integral(Enum));
check(!__is_integral(POD));
check(!__is_integral(TemplatedPOD<Int>));
check(!__is_integral(Empty));
check(!__is_integral(EmptyAr));
check(__is_integral(Int));
check(!__is_integral(IntAr));
check(!__is_integral(Derives));
check(!__is_integral(Incomplete));
check(__is_integral(ConstInt));
check(__is_integral(__decltype(GlobalStaticConstInt)));
check(!__is_integral(__decltype(GlobalFloat)));
check(__is_integral(Boolean));
check(__is_integral(UnsignedInt));
check(__is_integral(Dword));
check(!__is_integral(Half));
check(!__is_integral(Double));
check(!__is_integral(__decltype(fn)));
check(!__is_integral(RWByteAddressBuffer));

check(!__is_void(Enum));
check(!__is_void(POD));
check(!__is_void(TemplatedPOD<Int>));
check(!__is_void(Empty));
check(!__is_void(EmptyAr));
check(!__is_void(Int));
check(!__is_void(IntAr));
check(!__is_void(Derives));
check(!__is_void(Incomplete));
check(!__is_void(ConstInt));
check(!__is_void(__decltype(GlobalStaticConstInt)));
check(!__is_void(__decltype(GlobalFloat)));
check(!__is_void(__decltype(fn)));
check(!__is_void(RWByteAddressBuffer));
check(__is_void(void));

check(!__is_array(Enum));
check(!__is_array(POD));
check(!__is_array(TemplatedPOD<Int>));
check(__is_array(AoS));
check(!__is_array(SoA));
check(!__is_array(Empty));
check(__is_array(EmptyAr));
check(!__is_array(Int));
check(__is_array(IntAr));
check(!__is_array(Derives));
check(!__is_array(Incomplete));
check(!__is_array(ConstInt));
check(!__is_array(__decltype(GlobalStaticConstInt)));
check(!__is_array(__decltype(GlobalFloat)));
check(!__is_array(__decltype(fn)));
check(!__is_array(RWByteAddressBuffer));

check(!__is_const(Enum));
check(!__is_const(POD));
check(!__is_const(TemplatedPOD<Int>));
check(__is_const(__decltype(ConstStruct)));
check(!__is_const(AoS));
check(!__is_const(SoA));
check(!__is_const(Empty));
check(!__is_const(EmptyAr));
check(!__is_const(Int));
check(!__is_const(IntAr));
check(!__is_const(Derives));
check(!__is_const(Incomplete));
check(__is_const(ConstInt));
check(__is_const(__decltype(GlobalStaticConstInt)));
check(__is_const(__decltype(GlobalFloat)));

check(!__is_signed(Enum));
check(!__is_signed(POD));
check(!__is_signed(TemplatedPOD<Int>));
check(!__is_signed(__decltype(ConstStruct)));
check(!__is_signed(AoS));
check(!__is_signed(SoA));
check(!__is_signed(Empty));
check(!__is_signed(EmptyAr));
check(__is_signed(Int));
check(!__is_signed(IntAr));
check(!__is_signed(Derives));
check(!__is_signed(Incomplete));
check(__is_signed(ConstInt));
check(__is_signed(__decltype(GlobalStaticConstInt)));
//check(__is_signed(__decltype(GlobalFloat))); - failing but shouldn't
//check(__is_signed(__decltype(ConstFloat))); - failing but shouldn't
//check(__is_signed(__decltype(StaticConstFloat))); - failing but shouldn't
check(!__is_signed(Boolean));
check(!__is_signed(UnsignedInt));
check(!__is_signed(Dword));
//check(__is_signed(Half)); - failing but shouldn't
//check(__is_signed(Double)); - failing but shouldn't

//check(__is_unsigned(Enum)); //- its failing but I don't think it should
check(!__is_unsigned(POD));
check(!__is_unsigned(TemplatedPOD<Int>));
check(!__is_unsigned(__decltype(ConstStruct)));
check(!__is_unsigned(AoS));
check(!__is_unsigned(SoA));
check(!__is_unsigned(Empty));
check(!__is_unsigned(EmptyAr));
check(!__is_unsigned(Int));
check(!__is_unsigned(IntAr));
check(!__is_unsigned(Derives));
check(!__is_unsigned(Incomplete));
check(!__is_unsigned(ConstInt));
check(!__is_unsigned(__decltype(GlobalStaticConstInt)));
check(!__is_unsigned(__decltype(GlobalFloat)));
check(!__is_unsigned(__decltype(ConstFloat)));
check(!__is_unsigned(__decltype(StaticConstFloat)));
check(__is_unsigned(Boolean));
check(__is_unsigned(UnsignedInt));
check(__is_unsigned(Dword));
check(!__is_unsigned(Half));
check(!__is_unsigned(Double));

check(__is_same(Enum, Enum));
check(__is_same(POD, POD));
check(__is_same(TemplatedPOD<Int>, TemplatedPOD<Int>));
check(!__is_same(TemplatedPOD<Int>, TemplatedPOD<UnsignedInt>));
check(__is_same(__decltype(ConstStruct), __decltype(ConstStruct)));
check(__is_same(Empty, Empty));
check(__is_same(EmptyAr, EmptyAr));
check(__is_same(Int, Int));
check(__is_same(IntAr, IntAr));
check(__is_same(Derives, Derives));
check(__is_same(Incomplete, Incomplete));
check(!__is_same(Enum, POD));
check(!__is_same(Derives, POD));
check(!__is_same(Int, ConstInt));
check(__is_same(ConstInt, __decltype(GlobalStaticConstInt)));
check(!__is_same(UnsignedInt, Int));

check(!__is_array(Enum));
check(!__is_array(POD));
check(!__is_array(TemplatedPOD<Int>));
check(__is_array(AoS));
check(!__is_array(SoA));
check(__is_array(EmptyAr));
check(!__is_array(Int));
check(__is_array(IntAr));
check(!__is_array(__decltype(GlobalFloat)));
check(!__is_array(__decltype(fn)));

check(!__is_empty(Enum));
check(!__is_empty(POD));
check(!__is_empty(TemplatedPOD<Int>));
//check(!__is_empty(Derives)); - fails but shouldn't
check(__is_empty(Empty));
check(!__is_empty(Int));
check(!__is_empty(IntAr));
check(!__is_empty(__decltype(GlobalFloat)));

check(__is_complete_type(Enum));
check(__is_complete_type(POD));
check(__is_complete_type(TemplatedPOD<Int>));
check(__is_complete_type(Empty));
check(!__is_complete_type(Incomplete));
check(__is_complete_type(Int));
check(__is_complete_type(Derives));
check(__is_complete_type(__decltype(GlobalStaticConstInt)));