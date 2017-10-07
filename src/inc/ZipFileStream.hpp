#pragma once
#include "Exceptions.hpp"
#include "StreamBase.hpp"
#include "StreamBase.hpp"

#include <string>

namespace xPlat {

    // This represents a raw stream over a file contained in a .zip file.
    class ZipFileStream : public StreamBase
    {
    public:
        // TODO: define what streams to pass in on the .ctor
        ZipFileStream(
            std::uint32_t offset,
            std::uint32_t compressedSize,
            std::uint32_t uncompressedSize,
            bool isCompressed,
            StreamBase* stream
        ) :
            m_offset(offset),
            m_compressedSize(compressedSize),
            m_uncompressedSize(uncompressedSize),
            m_isCompressed(isCompressed),
            m_stream(stream)
        {
        }

        void Write(std::size_t size, const std::uint8_t* bytes) override;
        std::size_t Read(std::size_t size, const std::uint8_t* bytes) override;
        void Seek(std::uint64_t offset, Reference where) override;
        int Ferror() override;
        bool Feof() override;
        std::uint64_t Ftell()  override;

        bool IsCompressed();
        std::uint64_t GetCompressedSize();
        std::uint64_t GetUncompressedSize();

    protected:
        // TODO: change to uint64_t when adding 4+GB support
        std::uint64_t m_offset;
        std::uint64_t m_compressedSize;
        std::uint64_t m_uncompressedSize;

        bool m_isCompressed = false;
        std::uint64_t m_relativePosition = 0;
        StreamBase* m_stream = nullptr;
    };
}