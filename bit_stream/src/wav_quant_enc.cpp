#include "bit_stream.h"
#include "byte_stream.h"
#include <fstream>
#include <iostream>

using namespace std;

int main (int argc, char *argv[]) {	

    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input.wav> <output.enc>" << endl;
        return 1;
    }

    fstream inputFile_Wav {argv[argc -2], ios::in | ios::binary};
    fstream outputFile_Enc {argv[argc -1], ios::out | ios::binary};

    if (!inputFile_Wav.is_open() || !outputFile_Enc.is_open()) {
        cerr << "Error opening files" << endl;
        return 1;
    }

    BitStream obs(outputFile_Enc, STREAM_WRITE);
    
    int orig_bits = 16; // bits originais
    int n_bits = 8;     // bits após quantização
    
    uint16_t sample;

    // Lê 8 bytes apontados pelo sizeof e escreve o número de bits usados na quantização no início do arquivo
    while (inputFile_Wav.read(reinterpret_cast<char*>(&sample), sizeof(sample))) { 
        int quant_sample = sample >> (orig_bits - n_bits); // quantização
        obs.write_n_bits(quant_sample, n_bits);
    }
    
    obs.close();
    inputFile_Wav.close();
    outputFile_Enc.close();
    
    return 0;

}
