#include <iostream>
#include <vector>
#include <sndfile.h>
#include "Golomb.h"
#include <fstream>

// make bin/audio_decoder
// ./bin/audio_decoder wav_out/compressed.bin wav_out/output.wav

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " <input.bin> <output.wav>\n";
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    ifstream in(inputFile, ios::binary);
    if (!in.is_open()) {
        cerr << "Erro ao abrir ficheiro: " << inputFile << endl;
        return 1;
    }

    int sampleRate, numChannels, m;
    sf_count_t numFrames;

    in.read((char*)&sampleRate, sizeof(int));
    in.read((char*)&numChannels, sizeof(int));
    in.read((char*)&numFrames, sizeof(sf_count_t));
    in.read((char*)&m, sizeof(int));

    // Ler bits, que lê tudo com um iterador
    vector<char> fileBytes((istreambuf_iterator<char>(in)), {});
    in.close();

    string bitstream;
    // cada byte tem 8 bits
    bitstream.reserve(fileBytes.size() * 8);
    for (unsigned char b : fileBytes) {
        for (int i = 7; i >= 0; --i)
            // operação inversa (Byte para bits)
            // desloca À direita, dps pega o bit menos significativo
            bitstream += ((b >> i) & 1) ? '1' : '0';
    }

    // Descodificação  
    Golomb g(m, SignHandling::INTERLEAVING);
    vector<int16_t> samples(numFrames * numChannels);

    size_t index = 0;
    for (int i = 0; i < numFrames * numChannels; i++) {
        int prediction = 0;
        if (i >= numChannels)
            prediction = samples[i - numChannels];

        int residual = g.decode(bitstream, index);
        int reconstructed = prediction + residual;

        // limitar ao intervalo de 16 bits
        if (reconstructed > 32767) reconstructed = 32767;
        if (reconstructed < -32768) reconstructed = -32768;

        samples[i] = static_cast<int16_t>(reconstructed);
    }

    // Escrever WAV  
    SF_INFO sfInfoOut;
    sfInfoOut.samplerate = sampleRate;
    sfInfoOut.channels = numChannels;
    // combina formatos
    sfInfoOut.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

    SNDFILE* outFile = sf_open(outputFile, SFM_WRITE, &sfInfoOut);
    if (!outFile) {
        cerr << "Erro ao criar ficheiro WAV: " << outputFile << endl;
        return 1;
    }

    sf_write_short(outFile, samples.data(), samples.size());
    sf_close(outFile);

    cout << "Descodificação concluída: " << outputFile << endl;
    return 0;
}
