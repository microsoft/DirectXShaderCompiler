// RUN: %dxc -Tlib_6_8 -verify %s

void foo() {

WaveMatrixLeft<float, 16, 16> left[2];  // expected-error {{Array of 'WaveMatrixLeft' is not supported}}
WaveMatrixRight<float, 16, 16> right[2]; // expected-error {{Array of 'WaveMatrixRight' is not supported}}
WaveMatrixLeftColAcc<float, 16, 16> leftCol[2]; // expected-error {{Array of 'WaveMatrixLeftColAcc' is not supported}}
WaveMatrixRightRowAcc<float, 16, 16> rightRow[2]; // expected-error {{Array of 'WaveMatrixRightRowAcc' is not supported}}
WaveMatrixAccumulator<float, 16, 16> acc[2];  // expected-error {{Array of 'WaveMatrixAccumulator' is not supported}}

}
