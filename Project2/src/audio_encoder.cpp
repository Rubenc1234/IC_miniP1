#include <iostream>
#include <vector>
#include <sndfile.h>
#include "Golomb.h"
#include <fstream>

// make bin/audio_encoder
// ./bin/audio_encoder wav/sample.wav wav_out/compressed.bin 8

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Uso: " << argv[0] << " <input.wav> <output.bin> <m>\n";
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];
    int m = stoi(argv[3]);

    // Abrir ficheiro de áudio  
    SF_INFO sfInfo;
    SNDFILE* inFile = sf_open(inputFile, SFM_READ, &sfInfo);
    if (!inFile) {
        cerr << "Erro ao abrir ficheiro: " << inputFile << endl;
        return 1;
    }

    int numChannels = sfInfo.channels;
    int numSamples = sfInfo.frames * numChannels;
    //   Ler amostras  
    vector<int16_t> samples(numSamples);
    sf_read_short(inFile, samples.data(), numSamples);
    sf_close(inFile);

    // Cria objeto Golomb  
    Golomb g(m, SignHandling::INTERLEAVING);

    string bitstream;
    bitstream.reserve(numSamples * 10); // reserva aprox.

    // Codificação  
    for (int i = 0; i < numSamples; i++) {
        int prediction = 0;

        if (i >= numChannels) {
            // subtrai-se numChannels para obter a amostra anterior do mesmo canal
            prediction = samples[i - numChannels]; // predição temporal
        }

        int residual = samples[i] - prediction;
        bitstream += g.encode(residual);
    }

    // Gravar cabeçalho e bits  
    ofstream out(outputFile, ios::binary);
    out.write((char*)&sfInfo.samplerate, sizeof(int));
    out.write((char*)&numChannels, sizeof(int));
    // sf_count_t -> big numbers of frames
    out.write((char*)&sfInfo.frames, sizeof(sf_count_t));
    out.write((char*)&m, sizeof(int));

    // Escrever bits como bytes
    // coleta 8 bytes de cada vez para formar 1 Byte, e dps escreve
    uint8_t currentByte = 0;
    int bitCount = 0;
    for (char bit : bitstream) {
        // desloca bits 1 posição À esquerda e adiciona o novo bit no final
        currentByte = (currentByte << 1) | (bit == '1');
        bitCount++;
        if (bitCount == 8) {
            out.put(currentByte);
            bitCount = 0;
            currentByte = 0;
        }
    }
    // se sobrar bits insuficientes para formar 1 Byte acrescenta 0 à direita
    if (bitCount > 0) {
        currentByte <<= (8 - bitCount);
        out.put(currentByte);
    }

    out.close();
    cout << "Codificação concluída: " << outputFile << endl;
    return 0;
}
