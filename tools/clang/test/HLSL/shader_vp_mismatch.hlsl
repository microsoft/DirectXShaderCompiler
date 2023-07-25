// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s


[shader("vertex")]
[shader("pixel")]
[ numthreads( 64, 2, 2 ) ] /* expected-error {{Invalid shader stage attribute combination}} */
void VGMain() {
}
