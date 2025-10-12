#include "bit_stream.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cmath>
#include <fftw3.h>
#include <iomanip>
#include <cstdint>

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <input.wav> <output.enc> <níveis de quantização (bits)>" << endl;
        return 1;
    }

    const size_t bs = 1024;
    const int nChannels = 1;
    const double dctFrac = 0.2;
    const int n_bits = stoi(argv[3]);
    const uint32_t q_levels = 1u << n_bits;

    // Abre ficheiros
    fstream inputFile_Wav(argv[1], ios::in | ios::binary);
    fstream outputFile_Enc(argv[2], ios::out | ios::binary);
    
    if (!inputFile_Wav.is_open() || !outputFile_Enc.is_open()) {
        cerr << "Error opening files" << endl;
        return 1;
    }

    // Obtém tamanho do ficheiro e calcula número de frames
    inputFile_Wav.seekg(0, ios::end);
    uint64_t fileSize = static_cast<uint64_t>(inputFile_Wav.tellg());
    inputFile_Wav.seekg(44, ios::beg); // salta cabeçalho WAV


    uint64_t dataSize = fileSize-44;
    uint64_t nFrames = dataSize / (nChannels * 2); // 2 bytes por amostra

    cout << "dataSize = " << dataSize << " bytes" << endl;
    cout << "nFrames = " << nFrames << endl;
    

    // Lê todas as amostras
    vector<int16_t> samples(static_cast<size_t>(nFrames * nChannels));
    for (uint64_t f = 0; f < nFrames; ++f) {
        int16_t s = 0;
        // se WAV é stereo, lê 2 canais mas guarda só 1
        inputFile_Wav.read(reinterpret_cast<char*>(&s), sizeof(int16_t)); // canal 1
        samples[f] = s;

        inputFile_Wav.seekg(2, ios::cur); // pula canal 2
    }

    uint64_t nBlocks = nFrames / bs;
    cout << "nBlocks = " << nBlocks << endl;

    BitStream obs(outputFile_Enc, STREAM_WRITE);

    // Buffers de trabalho para DCT
    vector<double> x(bs);
    vector<double> X(bs);
    fftw_plan plan_d = fftw_plan_r2r_1d(static_cast<int>(bs), x.data(), X.data(), FFTW_REDFT10, FFTW_ESTIMATE);

    const size_t keep_sz = static_cast<size_t>(floor(bs * dctFrac));

    // Codificação bloco a bloco, todos os bits
    for (int c = 0; c < nChannels; ++c) {
        for (uint64_t b = 0; b < nBlocks/2 + 1; ++b) {
            // Copia bloco do canal c
            for (size_t k = 0; k < bs; ++k) {
                uint64_t frameIdx = b * bs + k;
                size_t idx = static_cast<size_t>(frameIdx) * nChannels + c;
                x[k] = static_cast<double>(samples[idx]) / 32768.0; // normaliza -1..1
                printf("x[%ld]=%f\n", k, x[k]); // DEBUG
            }

            // DCT
            fftw_execute(plan_d);

            // Após a primeira execução da DCT (por exemplo, só para o primeiro bloco)
            if (b == 0) {
                ofstream csv("dct_coeffs.csv");
                csv << "Index,Coefficient\n";
                for (size_t i = 0; i < bs; ++i) {
                    double coeff = X[i] / bs;
                    csv << i << "," << coeff << "\n";
                }
                csv.close();
                cout << "Ficheiro dct_coeffs.csv gerado com " << bs << " coeficientes." << endl;
            }



            // Quantização e escrita no BitStream
            for (size_t k = 0; k < keep_sz; ++k) {
                double val = X[k] / static_cast<double>(bs); // normalização DCT
                int q = static_cast<int>(round((val + 1.0) / 2.0 * (q_levels - 1))); // -1..1 -> 0..q_levels-1
                q = max(0, min(q, static_cast<int>(q_levels - 1)));
                printf("q[%lu]=%d\n", k, q); // DEBUG
                obs.write_n_bits(static_cast<uint64_t>(q), n_bits);
            }
        }
    }


    fftw_destroy_plan(plan_d);
    obs.close();
    inputFile_Wav.close();
    outputFile_Enc.close();

    // --- Calcular tamanho esperado em bytes ---
    size_t coef_per_block = static_cast<size_t>(floor(bs * dctFrac));
    size_t total_coef = nBlocks * coef_per_block * nChannels; // nChannels = 1
    size_t total_bits = total_coef * n_bits;
    size_t total_bytes = (total_bits + 7)/8; 


    cout << "Tamanho esperado do ficheiro .enc = " << total_bytes << " bytes" << endl;

    // --- Medir tamanho real do ficheiro .enc ---
    fstream testFile(argv[2], ios::in | ios::binary);
    testFile.seekg(0, ios::end);
    size_t real_size = static_cast<size_t>(testFile.tellg());
    testFile.close();

    cout << "dataSize = " << dataSize << endl;
    cout << "nChannels = " << nChannels << endl;
    cout << "nFrames = " << nFrames << endl;
    cout << "nBlocks = " << nBlocks << endl;
    cout << "bs * dctFrac = " << bs*dctFrac << endl;

    cout << "Tamanho real do ficheiro .enc = " << real_size << " bytes" << endl;

    // --- Comparação ---
    if (real_size == total_bytes) {
        cout << "OK: ficheiro codificado corretamente." << endl;
    } else {
        cout << "ATENÇÃO: tamanho real difere do esperado." << endl;
    }


    cout << "Encoding terminado. Ficheiro gerado: " << argv[2] << endl;
    return 0;
}
