// RUN: %dxc -T cs_6_0 %s | FileCheck %s

// Compiling this HLSL would trigger a false-positive assert:
//    
//    Error: assert(V[i]->getType() == Ty->getElementType() && "Wrong type in array element initializer")
//    File:
//    ..\..\third_party\dawn\third_party\dxc\lib\IR\Constants.cpp(886)
//    Func:   static llvm::ConstantArray::getImpl
//
// This assert has been disabled until this bug is resolved:
// https://github.com/microsoft/DirectXShaderCompiler/issues/5294

struct str {
  int i;
};

str func(inout str pointer) {
  return pointer;
}

static str P[4] = (str[4])0;

[numthreads(1, 1, 1)]
void main() {
  const str r = func(P[2]);
  return;
}
