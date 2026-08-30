#include "ProteomeTranslator.h"
#include <iostream>

std::string ProteomeTranslator::translateToProtein(const std::string& dnaSequence, int frame) {
    std::string protein = "M"; 
    size_t start = (frame >= 0 && frame < 3) ? frame : 0;
    for(size_t i = start; i + 2 < dnaSequence.length(); i += 3) {
        std::string codon = dnaSequence.substr(i, 3);
        if(codon == "TCT" || codon == "TCC") protein += "S"; 
        else if(codon == "GAA" || codon == "GAG") protein += "E"; 
        else if(codon == "GGT" || codon == "GGC") protein += "G"; 
        else protein += "X"; 
    }
    return protein;
}
