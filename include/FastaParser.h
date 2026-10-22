#ifndef FASTA_PARSER_H
#define FASTA_PARSER_H

#include <string>
#include <fstream>
#include <stdexcept>
#include <iostream>

class FastaParser {
public:
    // Constructor opens the file stream
    explicit FastaParser(const std::string& filepath);
    
    // Destructor ensures file closure (RAII)
    ~FastaParser();

    // Prevent copying to avoid double-closing file handles
    FastaParser(const FastaParser&) = delete;
    FastaParser& operator=(const FastaParser&) = delete;

    // Reads the next chunk of pure DNA sequence (strips headers/whitespace)
    // Returns true if data was read, false if EOF is reached.
    bool getNextChunk(std::string& chunk, size_t maxChunkSize = 4096, size_t overlapSize = 0);

private:
    std::ifstream fileStream;
    std::string overlapBuffer;
};

#endif // FASTA_PARSER_H
