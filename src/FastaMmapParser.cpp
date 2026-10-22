#include "FastaMmapParser.h"
#include <iostream>
#include <cctype>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

// Fast 256-byte branchless uppercase and whitespace lookup table
static constexpr uint8_t initFastaCharMap(uint8_t c) {
    return (c >= 'a' && c <= 'z') ? static_cast<uint8_t>(c - 32) :
           (c > ' ') ? c : 0;
}

#define ROW16(base) \
    initFastaCharMap(base+0),  initFastaCharMap(base+1),  initFastaCharMap(base+2),  initFastaCharMap(base+3),  \
    initFastaCharMap(base+4),  initFastaCharMap(base+5),  initFastaCharMap(base+6),  initFastaCharMap(base+7),  \
    initFastaCharMap(base+8),  initFastaCharMap(base+9),  initFastaCharMap(base+10), initFastaCharMap(base+11), \
    initFastaCharMap(base+12), initFastaCharMap(base+13), initFastaCharMap(base+14), initFastaCharMap(base+15)

static const uint8_t FASTA_CHAR_MAP[256] = {
    ROW16(0),   ROW16(16),  ROW16(32),  ROW16(48),
    ROW16(64),  ROW16(80),  ROW16(96),  ROW16(112),
    ROW16(128), ROW16(144), ROW16(160), ROW16(176),
    ROW16(192), ROW16(208), ROW16(224), ROW16(240)
};
#undef ROW16

static std::string resolveFastaPath(const std::string& path) {
    std::ifstream test(path);
    if (test.good()) return path;
    std::string fallback = "data/" + path;
    std::ifstream test2(fallback);
    if (test2.good()) return fallback;
    return path;
}

FastaMmapParser::FastaMmapParser(const std::string& filepath)
    : filepath(filepath), resolvedPath(resolveFastaPath(filepath)) {
    if (!initMmap()) {
        std::cout << "[!] Info: Fallback to std::ifstream for FASTA parsing.\n";
        fallbackStream.open(resolvedPath);
        if (!fallbackStream.is_open()) {
            throw std::runtime_error("FastaMmapParser Error: Could not open file " + resolvedPath);
        }
    } else {
        std::cout << "[+] Memory-Mapped Zero-Copy I/O active for " << resolvedPath << " (" << fileSize << " bytes mapped).\n";
    }
}

FastaMmapParser::~FastaMmapParser() {
    cleanupMmap();
    if (fallbackStream.is_open()) {
        fallbackStream.close();
    }
}

bool FastaMmapParser::initMmap() {
#ifdef _WIN32
    HANDLE hF = CreateFileA(resolvedPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hF == INVALID_HANDLE_VALUE) return false;

    DWORD sizeHigh = 0;
    DWORD sizeLow = GetFileSize(hF, &sizeHigh);
    fileSize = (static_cast<size_t>(sizeHigh) << 32) | sizeLow;

    if (fileSize == 0) {
        CloseHandle(hF);
        return false;
    }

    HANDLE hM = CreateFileMappingA(hF, NULL, PAGE_READONLY, 0, 0, NULL);
    if (!hM) {
        CloseHandle(hF);
        return false;
    }

    const char* pData = static_cast<const char*>(MapViewOfFile(hM, FILE_MAP_READ, 0, 0, 0));
    if (!pData) {
        CloseHandle(hM);
        CloseHandle(hF);
        return false;
    }

    hFile = hF;
    hMapping = hM;
    mappedData = pData;
    useMmap = true;
    return true;
#else
    fd = open(resolvedPath.c_str(), O_RDONLY);
    if (fd == -1) return false;

    struct stat sb;
    if (fstat(fd, &sb) == -1 || sb.st_size == 0) {
        close(fd);
        fd = -1;
        return false;
    }
    fileSize = static_cast<size_t>(sb.st_size);

    void* addr = mmap(NULL, fileSize, PROT_READ, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        fd = -1;
        return false;
    }

    mappedData = static_cast<const char*>(addr);
    useMmap = true;
    return true;
#endif
}

void FastaMmapParser::cleanupMmap() {
    if (useMmap && mappedData) {
#ifdef _WIN32
        UnmapViewOfFile(mappedData);
        if (hMapping) CloseHandle(static_cast<HANDLE>(hMapping));
        if (hFile) CloseHandle(static_cast<HANDLE>(hFile));
        hMapping = nullptr;
        hFile = nullptr;
#else
        munmap(const_cast<char*>(mappedData), fileSize);
        if (fd != -1) close(fd);
        fd = -1;
#endif
        mappedData = nullptr;
        useMmap = false;
    }
}

bool FastaMmapParser::getNextChunk(std::string& chunk, size_t maxChunkSize, size_t overlapSize) {
    chunk.clear();
    chunk.reserve(maxChunkSize);

    // Prepend overlap from previous chunk
    if (!overlapBuffer.empty()) {
        chunk = overlapBuffer;
        overlapBuffer.clear();
    }

    if (useMmap && mappedData) {
        while (chunk.size() < maxChunkSize && mappedOffset < fileSize) {
            char c = mappedData[mappedOffset++];

            // Header line handling
            if (c == '>') {
                while (mappedOffset < fileSize && mappedData[mappedOffset] != '\n') {
                    mappedOffset++;
                }
                if (mappedOffset < fileSize && mappedData[mappedOffset] == '\n') {
                    mappedOffset++;
                }
                continue;
            }

            uint8_t up = FASTA_CHAR_MAP[static_cast<uint8_t>(c)];
            if (up) {
                chunk.push_back(static_cast<char>(up));
            }
        }
    } else {
        char c;
        while (chunk.size() < maxChunkSize && fallbackStream.get(c)) {
            if (c == '>') {
                std::string headerLine;
                std::getline(fallbackStream, headerLine);
                continue;
            }
            uint8_t up = FASTA_CHAR_MAP[static_cast<uint8_t>(c)];
            if (up) {
                chunk.push_back(static_cast<char>(up));
            }
        }
    }

    // Save overlap for boundary detection
    bool hasMore = useMmap ? (mappedOffset < fileSize) : (!fallbackStream.eof());
    if (chunk.size() > overlapSize && hasMore) {
        overlapBuffer = chunk.substr(chunk.size() - overlapSize);
    }

    return !chunk.empty();
}
