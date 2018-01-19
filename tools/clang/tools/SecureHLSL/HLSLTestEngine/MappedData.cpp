#include "MappedData.h"

MappedData::MappedData() : m_pData(nullptr), m_size(0) 
{
}

MappedData::MappedData(ID3D12Resource *pResource, UINT32 sizeInBytes) : m_pData(nullptr), m_size(0) 
{
    ExecuteMap(pResource, sizeInBytes);
}

MappedData::~MappedData() 
{
    Reset();
}

void *MappedData::GetData() 
{ 
    return m_pData; 
}

UINT32 MappedData::GetSize() const 
{ 
    return m_size; 
}

void MappedData::Reset()
{
    if (_spResource != nullptr)
    {
        _spResource->Unmap(0, nullptr);
        _spResource.Reset();
    }

    m_pData = nullptr;
}

void MappedData::ExecuteMap(ID3D12Resource *pResource, UINT32 sizeInBytes)
{
    HRESULT hr;

    Reset();
    
    D3D12_RANGE ResourceRange;
    ResourceRange.Begin = 0;
    ResourceRange.End = sizeInBytes;
    IFT(pResource->Map(0, &ResourceRange, &m_pData));

    _spResource = pResource;
    m_size = sizeInBytes;
}
