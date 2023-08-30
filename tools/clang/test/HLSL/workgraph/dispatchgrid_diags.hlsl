// RUN: %clang_cc1 -fsyntax-only -verify %s
// NodeDispatchGrid and NodeMaxDispatchGrid validation diagnostics:
// - the x, y, z, component values must be in the range 1 to 2^16 - 1 (65,535) inclusive
// - the product x * y * z must not exceed 2^24 - 1 (16,777,215)
// - a warning should be generated for 2nd and subsequent occurances of these attributes

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(65535, 1, 1)]
[NumThreads(32, 1, 1)]
void node01()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 65536, 1)]       // expected-error {{'NodeDispatchGrid' Y component value must be between 1 and 65,535 (2^16-1) inclusive}}
[NumThreads(32, 1, 1)]
void node02()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(1, 1, 0)]           // expected-error {{'NodeDispatchGrid' Z component value must be between 1 and 65,535 (2^16-1) inclusive}}
[NumThreads(32, 1, 1)]
void node03()
{ }

static const int x = 1<<16;
static const uint y = 4;
static const int z = 0;

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(x, 256, 256)]       // expected-error {{'NodeDispatchGrid' X component value must be between 1 and 65,535 (2^16-1) inclusive}}
[NumThreads(32, 1, 1)]
void node04()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(256, 256, 256)]     // expected-error {{'NodeDispatchGrid' X * Y * Z product may not exceed 16,777,215 (2^24-1)}}
[NumThreads(32, 1, 1)]
void node05()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeDispatchGrid(64, 4, 2)]          // expected-warning {{attribute 'NodeDispatchGrid' is already applied}}
[NodeDispatchGrid(64, 4, 2)]
[NumThreads(32, 1, 1)]
void node06()
{}

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(65535, 1, 1)]
[NumThreads(32, 1, 1)]
void node11()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(1, 65536, 1)]    // expected-error {{'NodeMaxDispatchGrid' Y component value must be between 1 and 65,535 (2^16-1) inclusive}}
[NumThreads(32, 1, 1)]
void node12()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(1, 1, 0)]        // expected-error {{'NodeMaxDispatchGrid' Z component value must be between 1 and 65,535 (2^16-1) inclusive}}
[NumThreads(32, 1, 1)]
void node13()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(1, y, z)]        // expected-error {{'NodeMaxDispatchGrid' Z component value must be between 1 and 65,535 (2^16-1) inclusive}}
[NumThreads(32, 1, 1)]
void node14()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(1, 65535, 257)]  // expected-error {{'NodeMaxDispatchGrid' X * Y * Z product may not exceed 16,777,215 (2^24-1)}}
[NumThreads(32, 1, 1)]
void node15()
{ }

[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeMaxDispatchGrid(256, 8, 8)]      // expected-warning {{attribute 'NodeMaxDispatchGrid' is already applied}}
[NodeMaxDispatchGrid(64, 1, 1)]
[NumThreads(32, 1, 1)]
void node16()
{ }
