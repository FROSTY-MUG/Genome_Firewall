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

FastaMmapParser::FastaMmapParser(const std::string& filepath)
    : filepath(filepath) {
    if (!initMmap()) {
        std::cout << "[!] Info: Fallback to std::ifstream for FASTA parsing.\n";
        fallbackStream.open(filepath);
        if (!fallbackStream.is_open()) {
            throw std::runtime_error("FastaMmapParser Error: Could not open file " + filepath);
        }
    } else {
        std::cout << "[+] Memory-Mapped Zero-Copy I/O active for " << filepath << " (" << fileSize << " bytes mapped).\n";
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
    HANDLE hF = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
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

    hFile = static_cast<void*>(hF);
    hMapping = static_cast<void*>(hM);
    mappedData = pData;
    useMmap = true;
    mappedOffset = 0;
    return true;

#else
    int descriptor = open(filepath.c_str(), O_RDONLY);
    if (descriptor == -1) return false;

    struct stat sb;
    if (fstat(descriptor, &sb) == -1 || sb.st_size == 0) {
        close(descriptor);
        return false;
    }

    fileSize = static_cast<size_t>(sb.st_size);
    void* addr = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, descriptor, 0);
    if (addr == MAP_FAILED) {
        close(descriptor);
        return false;
    }

    fd = descriptor;
    mappedData = static_cast<const char*>(addr);
    useMmap = true;
    mappedOffset = 0;
    return true;
#endif
}

void FastaMmapParser::cleanupMmap() {
    if (!useMmap || !mappedData) return;

#ifdef _WIN32
    if (mappedData) UnmapViewOfFile(mappedData);
    if (hMapping) CloseHandle(static_cast<HANDLE>(hMapping));
    if (hFile) CloseHandle(static_cast<HANDLE>(hFile));
    hMapping = nullptr;
    hFile = nullptr;
#else
    if (mappedData != nullptr && mappedData != MAP_FAILED) munmap(const_cast<char*>(mappedData), fileSize);
    if (fd != -1) close(fd);
    fd = -1;
#endif

    mappedData = nullptr;
    useMmap = false;
}

bool FastaMmapParser::getNextChunk(std::string& chunk, size_t maxChunkSize, size_t overlapSize) {
    chunk.clear();
    chunk.reserve(maxChunkSize);

    if (useMmap && mappedData) {
        if (!overlapBuffer.empty()) {
            chunk = overlapBuffer;
            overlapBuffer.clear();
        }

        bool inHeader = false;
        while (chunk.size() < maxChunkSize && mappedOffset < fileSize) {
            char c = mappedData[mappedOffset++];

            if (c == '>') {
                inHeader = true;
                continue;
            }

            if (inHeader) {
                if (c == '\n' || c == '\r') {
                    inHeader = false;
                }
                continue;
            }

            if (!std::isspace(static_cast<unsigned char>(c))) {
                chunk.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
        }

        if (chunk.size() > overlapSize && mappedOffset < fileSize) {
            overlapBuffer = chunk.substr(chunk.size() - overlapSize);
        }

        return !chunk.empty();
    } else {
        // Fallback ifstream implementation
        if (!overlapBuffer.empty()) {
            chunk = overlapBuffer;
            overlapBuffer.clear();
        }

        char c;
        while (chunk.size() < maxChunkSize && fallbackStream.get(c)) {
            if (c == '>') {
                std::string headerLine;
                std::getline(fallbackStream, headerLine);
                continue;
            }

            if (!std::isspace(static_cast<unsigned char>(c))) {
                chunk.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
        }

        if (chunk.size() > overlapSize && !fallbackStream.eof()) {
            overlapBuffer = chunk.substr(chunk.size() - overlapSize);
        }

        return !chunk.empty();
    }
}
