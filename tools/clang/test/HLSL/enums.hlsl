// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

// CHECK: 1
// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

// CHECK: 1

enum Vertex {
    ZERO,
    ONE,
    TWO,
    THREE,
    FOUR,
    TEN = 10,
};

enum class VertexClass {
  ZEROC,
  ONEC,
  TWOC,
  THREEC,
  FOURC,
};

enum VertexBool : bool {
  ZEROB,
};

enum VertexInt : int {
  ZEROI,
  ONEI,
  TWOI,
  THREEI,
  FOURI,
  NEGONEI = -1,
};

enum VertexUInt : uint {
  ZEROU,
  ONEU,
  TWOU,
  THREEU,
  FOURU,
};

enum VertexDWord : dword {
  ZERODWORD,
};

enum Vertex64 : uint64_t {
  ZERO64,
  ONE64,
  TWO64,
  THREE64,
  FOUR64,
};

enum VertexMin16int : min16int {
  ZEROMIN16INT,
};

enum VertexMin16uint : min16uint {
  ZEROMIN16UINT,
};

enum VertexHalf : half {                                    /* expected-error {{non-integral type 'half' is an invalid underlying type}} */
  ZEROH,
};

enum VertexFloat : float {                                  /* expected-error {{non-integral type 'float' is an invalid underlying type}} */
  ZEROF,
};

enum VertexDouble : double {                                /* expected-error {{non-integral type 'double' is an invalid underlying type}} */
  ZEROD,
};

enum VertexMin16Float : min16float {                        /* expected-error {{non-integral type 'min16float' is an invalid underlying type}} */
  ZEROMIN16F,
};

enum VertexMin10Float : min10float {                        /* expected-error {{non-integral type 'min16float' is an invalid underlying type}} */
  ZEROMIN10F,
};

int getValueFromVertex(Vertex v) {                          /* expected-note {{candidate function not viable: no known conversion from 'Vertex64' to 'Vertex' for 1st argument}} expected-note {{candidate function not viable: no known conversion from 'VertexClass' to 'Vertex' for 1st argument}} expected-note {{candidate function not viable: no known conversion from 'VertexInt' to 'Vertex' for 1st argument}} expected-note {{candidate function not viable: no known conversion from 'VertexUInt' to 'Vertex' for 1st argument}} expected-note {{candidate function not viable: no known conversion from 'literal int' to 'Vertex' for 1st argument}} */
  switch (v) {
    case Vertex::ZERO:
      return 0;
    case Vertex::ONE:
      return 1;
    case Vertex::TWO:
      return 2;
    case Vertex::THREE:
      return 3;
    default:
      return -1;
  }
}

int getValueFromInt(int i) {
  switch (i) {
    case 0:
      return 0;
    case 1:
      return 1;
    case 2:
      return 2;
    default:
      return -1;
  }
}

int4 main() : SV_Target {
    int v0 = getValueFromInt(ZERO); //
    int v1 = getValueFromInt(VertexClass::ONEC); // TODO: WRONG
    int v2 = getValueFromInt(TWOI); //
    int v3 = getValueFromInt(THREEU); //
    int v4 = getValueFromInt(FOUR64); //

    int n0 = getValueFromVertex(ZERO);
    int n1 = getValueFromVertex(VertexClass::ONEC); /* expected-error {{no matching function for call to 'getValueFromVertex'}} */
    int n2 = getValueFromVertex(TWOI);              /* expected-error {{no matching function for call to 'getValueFromVertex'}} */
    int n3 = getValueFromVertex(THREEU);            /* expected-error {{no matching function for call to 'getValueFromVertex'}} */
    int n4 = getValueFromVertex(ZERO64);            /* expected-error {{no matching function for call to 'getValueFromVertex'}} */

    int n5 = getValueFromVertex(2);                 /* expected-error {{no matching function for call to 'getValueFromVertex'}} */

    Vertex cast0 = (Vertex) Vertex::FOUR;
    Vertex cast1 = (Vertex) VertexClass::THREEC;
    Vertex cast2 = (Vertex) VertexInt::TWOI;
    Vertex cast3 = (Vertex) Vertex64::ONE64;
    Vertex cast4 = (Vertex) VertexUInt::ZEROU;

    Vertex unary0 = Vertex::ZERO;
    VertexClass unary1 = VertexClass::ZEROC;
    unary0++;
    --unary1;

    Vertex castV = 1;                    /* expected-error {{cannot initialize a variable of type 'Vertex' with an rvalue of type 'literal int'}} */
    VertexInt castI = 10;                /* expected-error {{cannot initialize a variable of type 'VertexInt' with an rvalue of type 'literal int'}} */
    VertexClass castC = 52;              /* expected-error {{cannot initialize a variable of type 'VertexClass' with an rvalue of type 'literal int'}} */
    VertexUInt castU = 34;               /* expected-error {{cannot initialize a variable of type 'VertexUInt' with an rvalue of type 'literal int'}} */
    Vertex64 cast64 = 4037;              /* expected-error {{cannot initialize a variable of type 'Vertex64' with an rvalue of type 'literal int'}} */


    int unaryD = THREE++;                /* expected-error {{expression is not assignable}} */
    int unaryC = --VertexClass::FOURC;   /* expected-error {{expression is not assignable}} */
    int unaryI = ++TWOI;                 /* expected-error {{expression is not assignable}} */
    uint unaryU = ZEROU--;               /* expected-error {{expression is not assignable}} */
    int unary64 = ++THREE64;             /* expected-error {{expression is not assignable}} */


    int Iadd = Vertex::THREE - 48;
    int IaddI = VertexInt::ZEROI + 3;
    int IaddC = VertexClass::ONEC + 10; // WRONG
    int IaddU = VertexUInt::TWOU + 15;
    int Iadd64 = Vertex64::THREE64 - 67;

    float Fadd = Vertex::ONE + 1.5f;
    float FaddI = VertexInt::TWOI + 3.41f;
    float FaddC = VertexClass::THREEC - 256.0f; // WRONG
    float FaddU = VertexUInt::FOURU + 283.48f;
    float Fadd64 = Vertex64::ZERO64  - 8471.0f;
    
    return 1;
}