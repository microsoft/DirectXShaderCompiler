// RUN: %dxc /T vs_6_2 /E main %s | FileCheck %s

// CHECK: error: expected ';' after top level declarator
typedef int T : 1; // error: typedef member 'E' cannot be a bit-field
// CHECK: error: expected ';' after top level declarator
static int sb : 1; // error: static member 'sb' cannot be a bit-field

struct E {
  // CHECK: error: static member 'bf' cannot be a bit-field
  static uint bf : 8;
  // CHECK: error: named bit-field 'pad' has zero width
  uint pad : 0;
};

struct S {
  // CHECK: error: size of bit-field 'b' (33 bits) exceeds size of its type (32 bits)
  uint b : 33;
  // CHECK: error: size of bit-field 'c' (39 bits) exceeds size of its type (32 bits)
  bool c : 39;
  bool d : 3;
};

int a[sizeof(S) == 1 ? 1 : -1];

enum E {};

struct Z {};
typedef int Integer;

struct X {
  // clang: error: anonymous bit-field
  enum E : 1;
  // CHECK: error: non-integral type 'Z' is an invalid underlying type
  enum E : Z;
};

struct Y {
  // clang: error: anonymous bit-field
  enum E : int(2);
  // clang: error: 'Z' cannot have an explicit empty initializer
  enum E : Z();
};

enum WithUnderlying : uint { wu_value };
struct WithUnderlyingBitfield {
  WithUnderlying wu : 3;
} wu = { wu_value };
int want_unsigned(uint);
int check_enum_bitfield_promotes_correctly = want_unsigned(wu.wu);

struct A {
  uint bitX : 4;
  uint bitY : 4;
  uint var;
};

void main(A a : IN) {
  int x;
  // CHECK: error: invalid application of 'sizeof' to bit-field
  x = sizeof(a.bitX);
  x = sizeof((uint) a.bitX);
  // clang: error: invalid application of 'sizeof' to bit-field
  x = sizeof(a.var ? a.bitX : a.bitY);
  // clang: error: invalid application of 'sizeof' to bit-field
  x = sizeof(a.var ? a.bitX : a.bitX);
  // clang: error: invalid application of 'sizeof' to bit-field
  x = sizeof(a.bitX = 3);
  // CHECK: error: invalid application of 'sizeof' to bit-field
  x = sizeof(a.bitY += 3);
}
