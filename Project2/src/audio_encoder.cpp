#include <iostream>
#include <vector>
#include <sndfile.h>
#include "Golomb.h"
#include <fstream>
#include <numeric> // Para std::accumulate
#include <cmath>   // Para std::abs e std::log2
#include "utils.h"


// make bin/audio_encoder
// ./bin/audio_encoder wav/sample.wav wav_out/compressed.bin
// (Nota: 'm' já não é um argumento)

using namespace std;


int main(int argc, char* argv[]) {
    // CORREÇÃO: Removido o argumento 'm' da linha de comando
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <input.wav> <output.bin>\n";
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    // Abrir ficheiro de áudio
    SF_INFO sfInfo;
    SNDFILE* inFile = sf_open(inputFile, SFM_READ, &sfInfo);
    if (!inFile) {
        cerr << "Erro ao abrir ficheiro: " << inputFile << endl;
        return 1;
    }

    int numChannels = sfInfo.channels;
    sf_count_t numFrames = sfInfo.frames;
    int numSamples = numFrames * numChannels;
      
    vector<int16_t> samples(numSamples);
    sf_read_short(inFile, samples.data(), numSamples);
    sf_close(inFile);

    // --- NOVO: Análise de Duas Passagens (Passo 1: Calcular Resíduos) ---
    // Precisamos de calcular os resíduos primeiro para encontrar o 'm' ótimo.

    vector<int> residuals_ch1;
    vector<int> residuals_ch2; // Usado apenas para estéreo

    residuals_ch1.reserve(numFrames);
    if (numChannels == 2) {
        residuals_ch2.reserve(numFrames);
    }

    if (numChannels == 1) {
        // --- Processamento MONO ---
        int16_t prediction = 0;
        for (sf_count_t i = 0; i < numFrames; i++) {
            int residual = samples[i] - prediction;
            residuals_ch1.push_back(residual);
            prediction = samples[i]; // O preditor simples
        }
    } else if (numChannels == 2) {
        // --- NOVO: Processamento ESTÉREO (MID/SIDE) ---
        int16_t mid_pred = 0;
        int16_t side_pred = 0;
        for (sf_count_t i = 0; i < numFrames; i++) {
            int16_t L = samples[i * 2];
            int16_t R = samples[i * 2 + 1];

            // Conversão MID/SIDE (Predição Inter-Canal)
            int16_t mid = (L + R) / 2;
            int16_t side = L - R; // Usar L-R é mais simples que (L-R)/2 para reconstrução 100% lossless

            // Predição Temporal
            int res_mid = mid - mid_pred;
            int res_side = side - side_pred;

            residuals_ch1.push_back(res_mid);
            residuals_ch2.push_back(res_side);

            mid_pred = mid;
            side_pred = side;
        }
    } else {
        cerr << "Erro: Número de canais não suportado (apenas 1 ou 2).\n";
        return 1;
    }

    // --- NOVO: Calcular 'm' ótimo e criar Codificadores ---
    int m1 = calculate_optimal_m(residuals_ch1);
    int m2 = (numChannels == 2) ? calculate_optimal_m(residuals_ch2) : 0;
    
    Golomb g1(m1, SignHandling::INTERLEAVING);
    Golomb g2(m2 > 0 ? m2 : 1, SignHandling::INTERLEAVING); // g2 só é usada se for estéreo

    // --- NOVO: Gravar cabeçalho ---
    ofstream out(outputFile, ios::binary);
    out.write((char*)&sfInfo.samplerate, sizeof(int));
    out.write((char*)&numChannels, sizeof(int));
    out.write((char*)&numFrames, sizeof(sf_count_t));
    
    // CORREÇÃO: Escreve o(s) 'm' adaptativo(s) no cabeçalho
    out.write((char*)&m1, sizeof(int));
    if (numChannels == 2) {
        out.write((char*)&m2, sizeof(int));
    }
    
    cout << "Codificação (Canais=" << numChannels << "). M1=" << m1 << ", M2=" << m2 << endl;


    // --- NOVO: Análise de Duas Passagens (Passo 2: Codificar Resíduos) ---
    string bitstream;
    bitstream.reserve(numSamples * (m1 > 16 ? m1/2 : 8)); // Estimativa de reserva

    if (numChannels == 1) {
        for (int res : residuals_ch1) {
            bitstream += g1.encode(res);
        }
    } else {
        // Em estéreo, intercalamos os bits de MID e SIDE
        for (sf_count_t i = 0; i < numFrames; i++) {
            bitstream += g1.encode(residuals_ch1[i]);
            bitstream += g2.encode(residuals_ch2[i]);
        }
    }

    // Escrever bits como bytes (Lógica original mantida, pois está correta)
    uint8_t currentByte = 0;
    int bitCount = 0;
    for (char bit : bitstream) {
        currentByte = (currentByte << 1) | (bit == '1');
        bitCount++;
        if (bitCount == 8) {
            out.put(currentByte);
            bitCount = 0;
            currentByte = 0;
        }
    }
    if (bitCount > 0) {
        currentByte <<= (8 - bitCount);
        out.put(currentByte);
    }

    out.close();
    cout << "Codificação concluída: " << outputFile << endl;
    return 0;
}