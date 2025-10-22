#include "bit_stream.h"
#include "byte_stream.h"
#include <fstream>
#include <iostream>

using namespace std;

int main (int argc, char *argv[]) {	

    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <input.wav> <output.enc> <níveis de quantização>" << endl;
        return 1;
    }

    fstream inputFile_Wav {argv[1], ios::in | ios::binary};
    fstream outputFile_Enc {argv[2], ios::out | ios::binary};
    int n_bits = stoi(argv[3]);

    if (!inputFile_Wav.is_open() || !outputFile_Enc.is_open()) {
        cerr << "Error opening files" << endl;
        return 1;
    }

    BitStream obs(outputFile_Enc, STREAM_WRITE);
    
    int orig_bits = 16; // bits originais, usados pelo PCM por amostragem. "saber em quantos bits vou reconstituir (ampliar) e salvar o som num formato PCM válido e audível."
    
    uint16_t sample;

    // Saltar o cabeçalho WAV padrão (44 bytes)
    inputFile_Wav.seekg(44, ios::beg);


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
