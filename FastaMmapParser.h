#ifndef FASTA_MMAP_PARSER_H
#define FASTA_MMAP_PARSER_H

#include <string>
#include <fstream>
#include <cstddef>
#include <vector>

// Zero-Copy I/O Engine using Memory-Mapped Files (mmap / Win32 CreateFileMapping)
class FastaMmapParser {
public:
    explicit FastaMmapParser(const std::string& filepath);
    ~FastaMmapParser();

    FastaMmapParser(const FastaMmapParser&) = delete;
    FastaMmapParser& operator=(const FastaMmapParser&) = delete;

    // Get next chunk of sequence data with overlap support
    bool getNextChunk(std::string& chunk, size_t maxChunkSize = 4096, size_t overlapSize = 0);

    bool isMemoryMapped() const { return useMmap; }
    size_t getFileSize() const { return fileSize; }

private:
    std::string filepath;
    bool useMmap{false};
    size_t fileSize{0};

    // Mmap raw pointers & offset tracking
    const char* mappedData{nullptr};
    size_t mappedOffset{0};

#ifdef _WIN32
    void* hFile{nullptr};
    void* hMapping{nullptr};
#else
    int fd{-1};
#endif

    // Fallback stream parser
    std::ifstream fallbackStream;
    std::string overlapBuffer;

    void cleanupMmap();
    bool initMmap();
};

#endif // FASTA_MMAP_PARSER_H
