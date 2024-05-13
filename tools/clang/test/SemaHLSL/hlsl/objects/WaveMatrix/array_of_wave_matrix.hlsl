// RUN: %dxc -Tlib_6_8 -verify %s

void foo() {

WaveMatrixLeft<float, 16, 16> left[2];  // expected-error {{declaration of type WaveMatrixLeft may not be an array}}
WaveMatrixRight<float, 16, 16> right[2]; // expected-error {{declaration of type WaveMatrixRight may not be an array}}
WaveMatrixLeftColAcc<float, 16, 16> leftCol[2]; // expected-error {{declaration of type WaveMatrixLeftColAcc may not be an array}}
WaveMatrixRightRowAcc<float, 16, 16> rightRow[2]; // expected-error {{declaration of type WaveMatrixRightRowAcc may not be an array}}
WaveMatrixAccumulator<float, 16, 16> acc[2];  // expected-error {{declaration of type WaveMatrixAccumulator may not be an array}}

}

void bar(
WaveMatrixLeft<float, 16, 16> left[2],  // expected-error {{declaration of type WaveMatrixLeft may not be an array}}
WaveMatrixRight<float, 16, 16> right[2], // expected-error {{declaration of type WaveMatrixRight may not be an array}}
WaveMatrixLeftColAcc<float, 16, 16> leftCol[2], // expected-error {{declaration of type WaveMatrixLeftColAcc may not be an array}}
WaveMatrixRightRowAcc<float, 16, 16> rightRow[2], // expected-error {{declaration of type WaveMatrixRightRowAcc may not be an array}}
WaveMatrixAccumulator<float, 16, 16> acc[2]  // expected-error {{declaration of type WaveMatrixAccumulator may not be an array}}
) {
    
}

struct S {
WaveMatrixLeft<float, 16, 16> left[2];  // expected-error {{declaration of type WaveMatrixLeft may not be an array}}
WaveMatrixRight<float, 16, 16> right[2]; // expected-error {{declaration of type WaveMatrixRight may not be an array}}
WaveMatrixLeftColAcc<float, 16, 16> leftCol[2]; // expected-error {{declaration of type WaveMatrixLeftColAcc may not be an array}}
WaveMatrixRightRowAcc<float, 16, 16> rightRow[2]; // expected-error {{declaration of type WaveMatrixRightRowAcc may not be an array}}
WaveMatrixAccumulator<float, 16, 16> acc[2];  // expected-error {{declaration of type WaveMatrixAccumulator may not be an array}}
  
};
