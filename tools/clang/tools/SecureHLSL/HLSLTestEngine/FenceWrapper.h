#pragma once

#include "Common.h"

struct HANDLECloser
{
    void operator()(HANDLE hToClose)
    {
        ::CloseHandle(hToClose);
    }
};

class FenceWrapper
{
public:
    FenceWrapper() = delete;
    FenceWrapper(ID3D12Device *pDevice);

    void WaitForSignal(ID3D12CommandQueue *pCQ);

private:
    std::unique_ptr<void, HANDLECloser> m_fenceEvent;
    ComPtr<ID3D12Fence> m_spFence;
    UINT64 m_fenceValue;
};
