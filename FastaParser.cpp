#include "FastaParser.h"
#include <cctype>

FastaParser::FastaParser(const std::string& filepath) {
    fileStream.open(filepath);
    if (!fileStream.is_open()) {
        throw std::runtime_error("FastaParser Error: Failed to open file " + filepath);
    }
}

FastaParser::~FastaParser() {
    if (fileStream.is_open()) {
        fileStream.close();
    }
}

bool FastaParser::getNextChunk(std::string& chunk, size_t maxChunkSize, size_t overlapSize) {
    chunk.clear();
    chunk.reserve(maxChunkSize); // Prevent reallocation overhead

    // Prepend the overlap from the previous chunk
    if (!overlapBuffer.empty()) {
        chunk = overlapBuffer;
        overlapBuffer.clear();
    }

    char c;
    while (chunk.size() < maxChunkSize && fileStream.get(c)) {
        // If we hit a header, skip the rest of the line
        if (c == '>') {
            std::string headerLine;
            std::getline(fileStream, headerLine);
            continue;
        }

        // Only append valid non-whitespace characters
        if (!std::isspace(c)) {
            // Capitalize to ensure uniformity (a, c, t, g -> A, C, T, G)
            chunk.push_back(std::toupper(c));
        }
    }

    // Save the last 'overlapSize' characters for the next chunk.
    // Ensure we don't try to save more than we actually read!
    if (chunk.size() > overlapSize && !fileStream.eof()) {
        overlapBuffer = chunk.substr(chunk.size() - overlapSize);
        // We do NOT remove the overlap from the current chunk. 
        // It needs to be processed now, and re-processed at the start of the next chunk.
    }

    return !chunk.empty();
}
