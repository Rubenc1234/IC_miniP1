// wav_quant_enc_fixed.cpp
#include "bit_stream.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <fftw3.h>
#include <iomanip>
#include <cstdint>
#include <cstring>

using namespace std;

static uint16_t read_u16_le(ifstream &ifs) {
    uint8_t b0,b1; ifs.read(reinterpret_cast<char*>(&b0),1); ifs.read(reinterpret_cast<char*>(&b1),1);
    return static_cast<uint16_t>(b0 | (b1<<8));
}
static uint32_t read_u32_le(ifstream &ifs) {
    uint8_t b0,b1,b2,b3; ifs.read(reinterpret_cast<char*>(&b0),1); ifs.read(reinterpret_cast<char*>(&b1),1);
    ifs.read(reinterpret_cast<char*>(&b2),1); ifs.read(reinterpret_cast<char*>(&b3),1);
    return static_cast<uint32_t>(b0 | (b1<<8) | (b2<<16) | (b3<<24));
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <input.wav> <output.enc> <bits> <DCT_frac>" << endl;
        return 1;
    }

    const size_t bs = 1024;
    const double dctFrac = stod(argv[4]);
    const int n_bits = stoi(argv[3]);
    const uint32_t q_levels = 1u << n_bits;
    const size_t keep_sz = static_cast<size_t>(floor(bs * dctFrac));


    ifstream inputFile_Wav(argv[1], ios::in | ios::binary);
    fstream outputFile_Enc(argv[2], ios::out | ios::binary);

    if (!inputFile_Wav.is_open() || !outputFile_Enc.is_open()) {
        cerr << "Error opening files" << endl;
        return 1;
    }

    // --- HEADER ESCRITA --- Para identificar o keep_sz e bs na decodificação

    uint32_t magic = 0x44435431; // "DCT1" em ASCII
    uint16_t header_bs = static_cast<uint16_t>(bs);
    uint16_t header_keep = static_cast<uint16_t>(keep_sz);

    outputFile_Enc.write(reinterpret_cast<char*>(&magic), sizeof(magic));
    outputFile_Enc.write(reinterpret_cast<char*>(&header_bs), sizeof(header_bs));
    outputFile_Enc.write(reinterpret_cast<char*>(&header_keep), sizeof(header_keep));

    // Lê header WAV simples (assume PCM)
    inputFile_Wav.seekg(0, ios::beg);
    char riff[4]; inputFile_Wav.read(riff,4);
    if (strncmp(riff,"RIFF",4) != 0) { cerr << "Not a RIFF file\n"; return 1; }

    inputFile_Wav.seekg(22, ios::beg); 
    uint16_t numChannels = read_u16_le(inputFile_Wav); 
    inputFile_Wav.seekg(24, ios::beg); 
    uint32_t sampleRate = read_u32_le(inputFile_Wav); 
    inputFile_Wav.seekg(34, ios::beg);
    uint16_t bitsPerSample = read_u16_le(inputFile_Wav);

    // assume data chunk starts at 44 (simples)
    inputFile_Wav.seekg(0, ios::end);
    uint64_t fileSize = static_cast<uint64_t>(inputFile_Wav.tellg());
    uint64_t dataSize = fileSize - 44; 
    inputFile_Wav.seekg(44, ios::beg);

    cout << "WAV channels=" << numChannels << " sampleRate=" << sampleRate << " bitsPerSample=" << bitsPerSample << "\n";
    cout << "num_bits=" << n_bits << " q_levels=" << q_levels << "\n";
    cout << "dataSize = " << dataSize << " bytes\n";
    cout << "bs = " << bs << " keep_sz = " << keep_sz << " (frac=" << dctFrac << ")\n";
    cout << "File size = " << fileSize << " bytes\n";

    if (bitsPerSample != 16) { cerr << "Only 16-bit PCM supported\n"; return 1; }

    uint64_t nFrames = dataSize / (numChannels * 2); // F2
    cout << "nFrames = " << nFrames << endl;

    // Ler todos os samples (assume 16 bits PCM)
    // 1 Frame (2 Bytes por canal) = numChannels * 2 bytes
    // 1 Sample = 2 bytes (16 bits)

    vector<int16_t> samples(static_cast<size_t>(nFrames * numChannels));
    for (uint64_t f = 0; f < nFrames; ++f) {
        for (uint16_t ch = 0; ch < numChannels; ++ch) {
            int16_t s = 0;
            inputFile_Wav.read(reinterpret_cast<char*>(&s), sizeof(int16_t));
            samples[static_cast<size_t>(f) * numChannels + ch] = s;
            //printf("Sample[%llu][%u] = %d\n", f, ch, s);
        }
    }

    // nBlocks com arredondamento para cima
    size_t nBlocks = (nFrames + bs - 1) / bs; // F1
    cout << "nBlocks = " << nBlocks << endl;
    if (nBlocks == 0) { cerr << "File too short\n"; return 1; }

    BitStream obs(outputFile_Enc, STREAM_WRITE); // BitStream para escrita de bits

    vector<double> x(bs);
    vector<double> X(bs);
    fftw_plan plan_d = fftw_plan_r2r_1d(static_cast<int>(bs), x.data(), X.data(), FFTW_REDFT10, FFTW_ESTIMATE);

    cout << "keep_sz = " << keep_sz << "\n";

    uint64_t coeffs_written = 0; // Contador de coeficientes escritos

    // coder: bloco a bloco com zero-padding
    for (size_t b = 0; b < nBlocks; ++b) {

        // Preencher x com os samples do bloco b, canal 0, ou zeros se for padding

        for (size_t k = 0; k < bs; ++k) {
            uint64_t frameIdx = b*bs + k; // índice do frame global // F3
            if (frameIdx >= nFrames) {
                x[k] = 0.0; // padding final
            } else {
                size_t idx = frameIdx*numChannels + 0; // apenas canal 0
                x[k] = static_cast<double>(samples[idx]) / 32768.0; // normaliza para [-1.0, +1.0]
            }
        }

        fftw_execute(plan_d);

        // Ajuste da DCT-II para IDCT-III compatível

        for (size_t k = 0; k < keep_sz; ++k) {
            double val = X[k] / bs; // normalização
            // Quantização uniforme em [0 .. q_levels-1]

            int q = static_cast<int>(round((val + 1.0) / 2.0 * (q_levels - 1)));
            q = max(0, min(q, static_cast<int>(q_levels - 1)));

            //double fk = k * (44100.0 / static_cast<double>(bs)); // frequência aproximada
            //printf("Block %zu Coeff %zu: freq=%.2f Hz  DCT val=%.6f Quant=%d\n", b, k, fk, val, q);
            obs.write_n_bits(static_cast<uint64_t>(q), n_bits);
            coeffs_written++;
        }
    }

    fftw_destroy_plan(plan_d);
    obs.close();
    inputFile_Wav.close();
    outputFile_Enc.close();

    cout << "coeffs_written = " << coeffs_written << "\n";
    cout << "Encoding terminado. Ficheiro gerado: " << argv[2] << endl;
    return 0;
}
