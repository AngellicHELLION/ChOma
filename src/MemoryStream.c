#include "MemoryStream.h"

int memory_stream_read(MemoryStream *stream, uint64_t offset, size_t size, void *outBuf)
{
    if (stream->read) {
        int ret = stream->read(stream, offset, size, outBuf);
        if (ret != size) { return -1; }
        return 0;
    }
    return -1;
}

int memory_stream_write(MemoryStream *stream, uint64_t offset, size_t size, void *inBuf)
{
    if (stream->write) {
        int ret = stream->write(stream, offset, size, inBuf);
        if (ret != size) { return -1; }
        return 0;
    }
    return -1;
}

size_t memory_stream_get_size(MemoryStream *stream)
{
    if (stream->getSize) {
        size_t sizeOut;
        if (stream->getSize(stream, &sizeOut) == 0) return sizeOut;
    }
    return MEMORY_STREAM_SIZE_INVALID;
}

uint32_t memory_stream_get_flags(MemoryStream *stream)
{
    return stream->flags;
}

void _memory_stream_clone(MemoryStream *clone, MemoryStream *stream)
{
    clone->flags = stream->flags;

    clone->read = stream->read;
    clone->write = stream->write;
    clone->getSize = stream->getSize;

    clone->trim = stream->trim;
    clone->expand = stream->expand;

    clone->softclone = stream->softclone;
    clone->hardclone = stream->hardclone;
    clone->free = stream->free;
}

MemoryStream *memory_stream_softclone(MemoryStream *stream)
{
    if (stream->softclone) {
        MemoryStream *clone = stream->softclone(stream);
        if (clone) {
            _memory_stream_clone(clone, stream);
            return clone;
        }
    }
    return NULL;
}

MemoryStream *memory_stream_hardclone(MemoryStream *stream)
{
    if (stream->hardclone) {
        MemoryStream *clone = stream->hardclone(stream);
        if (clone) {
            _memory_stream_clone(clone, stream);
            return clone;
        }
    }
    return NULL;
}

int memory_stream_trim(MemoryStream *stream, size_t trimAtStart, size_t trimAtEnd)
{
    if (stream->trim) {
        return stream->trim(stream, trimAtStart, trimAtEnd);
    }
    return -1;
}

int memory_stream_expand(MemoryStream *stream, size_t expandAtStart, size_t expandAtEnd)
{
    if (stream->expand) {
        return stream->expand(stream, expandAtStart, expandAtEnd);
    }
    return -1;
}

void memory_stream_free(MemoryStream *stream)
{
    if (stream->free) {
        stream->free(stream);
    }
    free(stream);
}

#define COPY_DATA_BUFFER_SIZE 0x4000
int memory_stream_copy_data(MemoryStream *originStream, uint64_t originOffset, MemoryStream *targetStream, uint64_t targetOffset, size_t size)
{
    size_t originSize = memory_stream_get_size(originStream);
    size_t targetSize = memory_stream_get_size(targetStream);
    if (originSize == MEMORY_STREAM_SIZE_INVALID || targetSize == MEMORY_STREAM_SIZE_INVALID) return -1;

    if (originOffset + size > originSize) {
        return -1;
    }
    if (targetOffset + size > targetSize) {
        return -1;
    }

    uint8_t buffer[COPY_DATA_BUFFER_SIZE];
    for (uint32_t copiedSize = 0; copiedSize < size; copiedSize += COPY_DATA_BUFFER_SIZE) {
        uint32_t remainingSize = size - copiedSize;
        uint32_t sizeToCopy = COPY_DATA_BUFFER_SIZE;
        if (remainingSize < sizeToCopy) {
            sizeToCopy = remainingSize;
        }
        int rr = memory_stream_read(originStream, originOffset + copiedSize, sizeToCopy, buffer);
        if (rr != 0) return rr;
        int wr = memory_stream_write(targetStream, targetOffset + copiedSize, sizeToCopy, buffer);
        if (wr != 0) return wr;
    }

    return 0;
}

int memcmp_masked(const void *str1, const void *str2, unsigned char* mask, size_t n)
{
    const unsigned char* p = (const unsigned char*)str1;
    const unsigned char* q = (const unsigned char*)str2;

    if(p == q) return 0;

    for(int i = 0; i < n; i++)
    {
        unsigned char cMask = 0xFF;
        if (mask) {
            cMask = mask[i];
        }
        if((p[i] & cMask) != (q[i] & cMask))
        {
            // we do not care about 1 / -1
            return -1;
        }
    }

    return 0;
}

int memory_stream_find_memory(MemoryStream *stream, uint64_t searchOffset, size_t searchSize, void *bytes, void *mask, size_t nbytes, uint16_t alignment, uint64_t *foundOffsetOut)
{
    if (nbytes % alignment != 0) return 0;
    if (nbytes == 0) return 0;

    uint8_t *bytesC = bytes;
    uint8_t *maskC = mask;

    size_t nbytesMatched = 0;
    for (uint64_t curOffset = searchOffset; curOffset < (searchOffset + searchSize); curOffset += alignment) {
        uint8_t *curCheckMask = NULL;
        if (maskC) {
            curCheckMask = &maskC[nbytesMatched];
        }
        uint8_t *curCheckBytes = &bytesC[nbytesMatched];

        uint8_t curReadBytes[alignment];
        memory_stream_read(stream, curOffset, alignment, curReadBytes);
        if (!memcmp_masked(curReadBytes, curCheckBytes, curCheckMask, alignment)) {
            nbytesMatched += alignment;
        }
        else {
            nbytesMatched = 0;
        }
        if (nbytesMatched >= nbytes) {
            *foundOffsetOut = curOffset - (nbytesMatched - alignment);
            return 0;
        }
    }
    return -1;
}