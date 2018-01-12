#include "FenceWrapper.h"

FenceWrapper::FenceWrapper(ID3D12Device *pDevice)
{
    HRESULT hr;

    m_fenceValue = 1;
    IFT(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_spFence)));

    m_fenceEvent.reset(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
    if (m_fenceEvent == nullptr)
    {
        IFT(HRESULT_FROM_WIN32(GetLastError()));
    }
}

void FenceWrapper::WaitForSignal(ID3D12CommandQueue *pCQ)
{
    HRESULT hr;

    IFT(pCQ->Signal(m_spFence.Get(), m_fenceValue));

    if (m_spFence->GetCompletedValue() < m_fenceValue)
    {
        IFT(m_spFence->SetEventOnCompletion(m_fenceValue, m_fenceEvent.get()));
        ::WaitForSingleObject(m_fenceEvent.get(), INFINITE);
    }

    m_fenceValue++;
}
