// RUN: %clang_cc1 -fsyntax-only -ffreestanding -verify %s

RWByteAddressBuffer NonGCBuf;
globallycoherent RWByteAddressBuffer GCBuf;

RWByteAddressBuffer getNonGCBuf() {
  return NonGCBuf;
}

globallycoherent RWByteAddressBuffer getGCBuf() { 
  return GCBuf;
}

RWByteAddressBuffer getNonGCGCBuf() {
  return GCBuf; // expected-warning{{implicit conversion from 'globallycoherent RWByteAddressBuffer' to 'RWByteAddressBuffer' loses globallycoherent annotation}}
}

globallycoherent RWByteAddressBuffer getGCNonGCBuf() {
  return NonGCBuf; // expected-warning{{implicit conversion from 'RWByteAddressBuffer' to 'globallycoherent RWByteAddressBuffer' adds globallycoherent annotation}}
}

void NonGCStore(RWByteAddressBuffer Buf) {
  Buf.Store(0, 0);
}

void GCStore(globallycoherent RWByteAddressBuffer Buf) {
  Buf.Store(0, 0);
}

[numthreads(1, 1, 1)]
void main()
{
  NonGCStore(NonGCBuf); // No diagnostic
  GCStore(NonGCBuf); // expected-warning{{implicit conversion from 'RWByteAddressBuffer' to 'globallycoherent RWByteAddressBuffer' adds globallycoherent annotation}}
  NonGCStore(GCBuf); // expected-warning{{implicit conversion from 'globallycoherent RWByteAddressBuffer' to 'RWByteAddressBuffer' loses globallycoherent annotation}}
  GCStore(GCBuf); // No diagnostic

  RWByteAddressBuffer NonGCCopyNonGC = NonGCBuf; // No diagnostic
  RWByteAddressBuffer NonGCCopyGC = GCBuf; // expected-warning{{implicit conversion from 'globallycoherent RWByteAddressBuffer' to 'RWByteAddressBuffer' loses globallycoherent annotation}}

  globallycoherent RWByteAddressBuffer GCCopyNonGC = NonGCBuf; // expected-warning{{implicit conversion from 'RWByteAddressBuffer' to 'globallycoherent RWByteAddressBuffer' adds globallycoherent annotation}}
  globallycoherent RWByteAddressBuffer GCCopyGC = GCBuf; // No diagnostic

  globallycoherent RWByteAddressBuffer GCCopyNonGCReturn = getNonGCBuf(); // expected-warning{{implicit conversion from 'RWByteAddressBuffer' to 'globallycoherent RWByteAddressBuffer' adds globallycoherent annotation}}

   RWByteAddressBuffer NonGCCopyGCReturn = getGCBuf(); // expected-warning{{implicit conversion from 'globallycoherent RWByteAddressBuffer' to 'RWByteAddressBuffer' loses globallycoherent annotation}}
}
