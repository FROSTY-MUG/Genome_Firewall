#ifndef PROTEOME_TRANSLATOR_H
#define PROTEOME_TRANSLATOR_H

#include <string>
#include <vector>
#include <unordered_map>

class ProteomeTranslator {
public:
    // Translate a single DNA sequence (in the 5'->3' direction) in a given frame (0, 1, or 2)
    static std::string translateToProtein(const std::string& dna, int frame = 0);

    // Get the reverse complement of a DNA sequence
    static std::string reverseComplement(const std::string& dna);

    // Generate all 6 reading frames (3 forward: 0,1,2 and 3 reverse complement: 0,1,2)
    static std::vector<std::string> translateAllFrames(const std::string& dna);

    // Individual codon translation
    static char codonToAminoAcid(const std::string& codon);

private:
    static const std::unordered_map<std::string, char> codonMap;
};

#endif // PROTEOME_TRANSLATOR_H
