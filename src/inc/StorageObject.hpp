//
//  Copyright (C) 2017 Microsoft.  All rights reserved.
//  See LICENSE file in the project root for full license information.
// 
#pragma once

#include "Exceptions.hpp"
#include "StreamBase.hpp"
#include "FileStream.hpp"
#include "ComHelper.hpp"

#include <string>
#include <vector>
#include <memory>
#include <utility>

enum class FileNameOptions : std::uint16_t
{
    FootPrintOnly = 0x1,
    PayloadOnly = 0x2,
    All = 0x3,
};

inline constexpr FileNameOptions operator &(FileNameOptions a, FileNameOptions b)
{
    return static_cast<FileNameOptions>(static_cast<uint16_t>(a) & static_cast<uint16_t>(b));
}

inline constexpr FileNameOptions operator |(FileNameOptions a, FileNameOptions b)
{
    return static_cast<FileNameOptions>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

// internal interface
// {ec25b96e-0db1-4483-bdb1-cab1109cb741}
#ifndef WIN32
interface IStorageObject : public IUnknown
#else
#include "Unknwn.h"
#include "Objidl.h"
class IStorageObject : public IUnknown
#endif
{
public:        
    virtual const char* GetPathSeparator() = 0;

    // Obtains a vector of UTF-8 formatted string names contained in the storage object
    virtual std::vector<std::string> GetFileNames(FileNameOptions options) = 0;

    // Obtains a pointer to a stream representing the file that exists in the storage object
    virtual MSIX::ComPtr<IStream> GetFile(const std::string& fileName) = 0;

    // Opens a stream to a file by name in the storage object.  If the file does not exist and mode is read,
    // or read + update, then nullptr is returned.  If the file is opened with write and it does not exist, 
    // then the file is created and an empty stream to the file is handed back to the caller.
    virtual MSIX::ComPtr<IStream> OpenFile(const std::string& fileName, MSIX::FileStream::Mode mode) = 0;

    // Returns the file name of the storage object.
    virtual std::string GetFileName() = 0;
};
MSIX_INTERFACE(IStorageObject, 0xec25b96e,0x0db1,0x4483,0xbd,0xb1,0xca,0xb1,0x10,0x9c,0xb7,0x41);
