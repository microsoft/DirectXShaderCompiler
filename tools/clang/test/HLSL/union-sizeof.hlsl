// RUN: %clang_cc1 -Wno-unused-value -fsyntax-only -ffreestanding -verify -verify-ignore-unexpected=note %s

// Tests usage of the sizeof operator

union EmptyStruct {};
union SimpleStruct { int x; };
union StructWithResource { Buffer buf; int x; };

void main()
{
  // Type vs expression argument
  sizeof(int);
  sizeof((int)0);

  // Type shapes
  sizeof(int);
  sizeof(int2);
  sizeof(int2x2);
  sizeof(int[2]);
  sizeof(SimpleStruct);
  sizeof(EmptyStruct);
}
