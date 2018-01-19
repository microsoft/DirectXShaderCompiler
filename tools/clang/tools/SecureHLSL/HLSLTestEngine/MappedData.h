#pragma once

#include "Common.h"

class MappedData
{
public:
    MappedData();
    MappedData(ID3D12Resource *pResource, UINT32 sizeInBytes);
    ~MappedData();
    
    void *GetData();
    UINT32 GetSize() const;
    void Reset();
    
private:
    void ExecuteMap(ID3D12Resource *pResource, UINT32 sizeInBytes);

private:
    ComPtr<ID3D12Resource> _spResource;
    void *m_pData;
    UINT32 m_size;
};
