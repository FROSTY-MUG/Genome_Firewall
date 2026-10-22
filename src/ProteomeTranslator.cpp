#include "ProteomeTranslator.h"
#include <algorithm>

const std::unordered_map<std::string, char> ProteomeTranslator::codonMap = {
    // Phenylalanine & Leucine
    {"TTT", 'F'}, {"TTC", 'F'}, {"TTA", 'L'}, {"TTG", 'L'},
    {"CTT", 'L'}, {"CTC", 'L'}, {"CTA", 'L'}, {"CTG", 'L'},
    // Isoleucine & Methionine
    {"ATT", 'I'}, {"ATC", 'I'}, {"ATA", 'I'}, {"ATG", 'M'},
    // Valine
    {"GTT", 'V'}, {"GTC", 'V'}, {"GTA", 'V'}, {"GTG", 'V'},
    // Serine
    {"TCT", 'S'}, {"TCC", 'S'}, {"TCA", 'S'}, {"TCG", 'S'},
    // Proline
    {"CCT", 'P'}, {"CCC", 'P'}, {"CCA", 'P'}, {"CCG", 'P'},
    // Threonine
    {"ACT", 'T'}, {"ACC", 'T'}, {"ACA", 'T'}, {"ACG", 'T'},
    // Alanine
    {"GCT", 'A'}, {"GCC", 'A'}, {"GCA", 'A'}, {"GCG", 'A'},
    // Tyrosine & Stop
    {"TAT", 'Y'}, {"TAC", 'Y'}, {"TAA", '*'}, {"TAG", '*'},
    // Histidine & Glutamine
    {"CAT", 'H'}, {"CAC", 'H'}, {"CAA", 'Q'}, {"CAG", 'Q'},
    // Asparagine & Lysine
    {"AAT", 'N'}, {"AAC", 'N'}, {"AAA", 'K'}, {"AAG", 'K'},
    // Aspartate & Glutamate
    {"GAT", 'D'}, {"GAC", 'D'}, {"GAA", 'E'}, {"GAG", 'E'},
    // Cysteine, Stop & Tryptophan
    {"TGT", 'C'}, {"TGC", 'C'}, {"TGA", '*'}, {"TGG", 'W'},
    // Arginine
    {"CGT", 'R'}, {"CGC", 'R'}, {"CGA", 'R'}, {"CGG", 'R'},
    // Serine & Arginine
    {"AGT", 'S'}, {"AGC", 'S'}, {"AGA", 'R'}, {"AGG", 'R'},
    // Glycine
    {"GGT", 'G'}, {"GGC", 'G'}, {"GGA", 'G'}, {"GGG", 'G'}
};

char ProteomeTranslator::codonToAminoAcid(const std::string& codon) {
    if (codon.length() != 3) return 'X';
    std::string upperCodon = codon;
    for (char& c : upperCodon) {
        if (c >= 'a' && c <= 'z') c -= 32;
    }
    auto it = codonMap.find(upperCodon);
    return (it != codonMap.end()) ? it->second : 'X';
}

std::string ProteomeTranslator::translateToProtein(const std::string& dna, int frame) {
    if (dna.length() < 3) return "";
    size_t start = (frame >= 0 && frame < 3) ? static_cast<size_t>(frame) : 0;
    std::string protein;
    protein.reserve((dna.length() - start) / 3);

    for (size_t i = start; i + 2 < dna.length(); i += 3) {
        std::string codon = dna.substr(i, 3);
        protein.push_back(codonToAminoAcid(codon));
    }
    return protein;
}

std::string ProteomeTranslator::reverseComplement(const std::string& dna) {
    std::string rc;
    rc.reserve(dna.length());
    for (auto it = dna.rbegin(); it != dna.rend(); ++it) {
        char c = *it;
        switch (c) {
            case 'A': case 'a': rc.push_back('T'); break;
            case 'T': case 't': rc.push_back('A'); break;
            case 'C': case 'c': rc.push_back('G'); break;
            case 'G': case 'g': rc.push_back('C'); break;
            default:            rc.push_back(c);   break;
        }
    }
    return rc;
}

std::vector<std::string> ProteomeTranslator::translateAllFrames(const std::string& dna) {
    std::vector<std::string> frames;
    frames.reserve(6);

    // 3 forward reading frames (0, 1, 2)
    frames.push_back(translateToProtein(dna, 0));
    frames.push_back(translateToProtein(dna, 1));
    frames.push_back(translateToProtein(dna, 2));

    // 3 reverse complement reading frames (0, 1, 2)
    std::string rc = reverseComplement(dna);
    frames.push_back(translateToProtein(rc, 0));
    frames.push_back(translateToProtein(rc, 1));
    frames.push_back(translateToProtein(rc, 2));

    return frames;
}
