// RUN: %dxc -T cs_6_6 %s | %FileCheck %s

// Make sure only 2 annotate handle, 1 mark glc, 1 does not mark.
// CHECK-DAG:%[[noglc:.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 4107, i32 0 })
// CHECK-DAG:%[[glc:.*]] = call %dx.types.Handle @dx.op.annotateHandle(i32 216, %dx.types.Handle %{{.*}}, %dx.types.ResourceProperties { i32 20491, i32 0 })
// CHECK:call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %[[glc]], i32 1, i32 undef, i32 1, i32 undef, i32 undef, i32 undef, i8 1, i32 4)
// CHECK:call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %[[noglc]], i32 0, i32 undef, i32 0, i32 undef, i32 undef, i32 undef, i8 1, i32 4)
// CHECK:call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %[[glc]], i32 1, i32 undef, i32 1, i32 undef, i32 undef, i32 undef, i8 1, i32 4)
// CHECK:call void @dx.op.rawBufferStore.i32(i32 140, %dx.types.Handle %[[noglc]], i32 0, i32 undef, i32 0, i32 undef, i32 undef, i32 undef, i8 1, i32 4)

static globallycoherent RWByteAddressBuffer Buffer0 = ResourceDescriptorHeap[0];

void Foo(RWByteAddressBuffer Buffer1)
{
    Buffer1.Store(0, 0);	// ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
}

void Bar(globallycoherent RWByteAddressBuffer Buffer1)
{
    Buffer1.Store(1, 1);	// ; AnnotateHandle(res,props)  resource: globallycoherent RWByteAddressBuffer
}

[numthreads(1, 1, 1)]
void main()
{
	Buffer0.Store(1, 1);	// ; AnnotateHandle(res,props)  resource: globallycoherent RWByteAddressBuffer
    Foo(Buffer0);
    Bar(Buffer0);

    RWByteAddressBuffer Buffer1 = Buffer0;
    Buffer1.Store(0, 0);	// ; AnnotateHandle(res,props)  resource: RWByteAddressBuffer
}