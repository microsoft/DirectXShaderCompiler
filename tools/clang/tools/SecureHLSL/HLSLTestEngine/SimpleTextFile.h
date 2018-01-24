#pragma once

#include "Common.h"

class SimpleTextFile
{
public:
    SimpleTextFile(const wchar_t* pszFilename);
    ~SimpleTextFile();

    void ReadFile(std::vector<char> &fileBytes);
    void ReadFile(std::vector<wchar_t> &fileBytes);

private:
    HANDLE _hFile;
};
