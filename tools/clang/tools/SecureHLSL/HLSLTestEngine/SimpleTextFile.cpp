#include "SimpleTextFile.h"

SimpleTextFile::SimpleTextFile(const wchar_t* pszFilename)
{
    HRESULT hr;

    _hFile = ::CreateFile(pszFilename, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (_hFile == nullptr)
    {
        IFT(E_FAIL);
    }
}

SimpleTextFile::~SimpleTextFile()
{
    if (_hFile != nullptr)
    {
        ::CloseHandle(_hFile);
    }
}

void SimpleTextFile::ReadFile(std::vector<char> &fileBytes)
{
    HRESULT hr;

    LARGE_INTEGER fileSize;
    if (!::GetFileSizeEx(_hFile, &fileSize))
    {
        IFT(E_FAIL);
    }

    if (fileSize.HighPart != 0)
    {
        // Not expecting gigantic HLSL files
        IFT(E_UNEXPECTED);
    }

    // We know now how much we need to store the whole file
    fileBytes.resize(fileSize.LowPart + 2);

    DWORD dwRead;
    if (!::ReadFile(_hFile, fileBytes.data(), fileSize.LowPart, &dwRead, nullptr))
    {
        IFT(E_FAIL);
    }

    // Terminate file data so it can be cast to a string
    fileBytes[fileSize.LowPart] = 0;
    fileBytes[fileSize.LowPart + 1] = 0;
}

void SimpleTextFile::ReadFile(std::vector<wchar_t> &fileBytes)
{
    HRESULT hr;

    // First read raw data
    std::vector<char> rawFile;
    ReadFile(rawFile);

    // Find out the needed buffer size
    const int utf16Length = ::MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        rawFile.data(),
        -1,
        nullptr,
        0);

    if (utf16Length == 0)
    {
        IFT(E_FAIL);
    }

    fileBytes.resize(utf16Length);

    if (!::MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        rawFile.data(),
        -1,
        fileBytes.data(),
        static_cast<int>(fileBytes.size())))
    {
        IFT(E_FAIL);
    }
}
