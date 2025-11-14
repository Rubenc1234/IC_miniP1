#include <iostream>
#include <vector>
#include <sndfile.h>
#include "Golomb.h"
#include <fstream>
#include <numeric> 
#include <cmath>   
#include "utils.h" //

using namespace std;

// Função principal para codificar áudio WAV em formato binário comprimido
int main(int argc, char* argv[]) {
    // Verificação dos argumentos de linha de comando
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <input.wav> <output.bin>\n";
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    // Abertura e leitura do ficheiro WAV de entrada
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

    // Escrita dos metadados (taxa de amostragem, canais, frames) no ficheiro de saída
    ofstream out(outputFile, ios::binary);
    out.write((char*)&sfInfo.samplerate, sizeof(int));
    out.write((char*)&numChannels, sizeof(int));
    out.write((char*)&numFrames, sizeof(sf_count_t));
    
    cout << "Codificação (Canais=" << numChannels << ", m adaptativo por bloco, Ordem 1)\n";

    string bitstream;
    const int blockSize = 4096; 

    int16_t mono_pred = 0;
    int16_t mid_pred = 0;
    int16_t side_pred = 0;

    // Processamento por blocos para codificação adaptativa
    for (sf_count_t frame_start = 0; frame_start < numFrames; frame_start += blockSize) {
        sf_count_t frame_end = min(frame_start + blockSize, numFrames);
        sf_count_t framesInBlock = frame_end - frame_start;
        
        // Cálculo dos resíduos preditivos para o bloco atual
        vector<int> block_residuals_ch1;
        vector<int> block_residuals_ch2;
        block_residuals_ch1.reserve(framesInBlock);
        if (numChannels == 2) block_residuals_ch2.reserve(framesInBlock);

        int16_t temp_mono_pred = mono_pred;
        int16_t temp_mid_pred = mid_pred;
        int16_t temp_side_pred = side_pred;
        
        for (sf_count_t i = 0; i < framesInBlock; i++) {
            sf_count_t frame_index = frame_start + i;
            
            if (numChannels == 1) {
                // Predição temporal para mono: diferença entre amostra atual e preditor anterior
                int16_t current_sample = samples[frame_index];
                int residual = current_sample - temp_mono_pred;
                block_residuals_ch1.push_back(residual);
                temp_mono_pred = current_sample; 
            } else { // numChannels == 2
                // Predição inter-canal para estéreo: cálculo de mid e side, com predição temporal
                int16_t L = samples[frame_index * 2];
                int16_t R = samples[frame_index * 2 + 1];
                int16_t mid = (L + R) / 2;
                int16_t side = L - R; 

                int res_mid = mid - temp_mid_pred;
                int res_side = side - temp_side_pred;
                block_residuals_ch1.push_back(res_mid);
                block_residuals_ch2.push_back(res_side);

                temp_mid_pred = mid;
                temp_side_pred = side;
            }
        }
        
        // Determinação adaptativa do parâmetro m ótimo para o bloco
        int m1 = calculate_optimal_m(block_residuals_ch1); 
        
        bitstream += int_to_binary_string(m1, 16); 
        
        Golomb g1(m1, SignHandling::INTERLEAVING);
        Golomb* g2 = nullptr;

        if (numChannels == 2) {
            int m2 = calculate_optimal_m(block_residuals_ch2); 
            bitstream += int_to_binary_string(m2, 16); 
            g2 = new Golomb(m2 > 0 ? m2 : 1, SignHandling::INTERLEAVING);
        }

        // Codificação Golomb dos resíduos
        if (numChannels == 1) {
            for (int res : block_residuals_ch1) {
                bitstream += g1.encode(res);
            }
        } else {
            for (size_t i = 0; i < block_residuals_ch1.size(); ++i) {
                bitstream += g1.encode(block_residuals_ch1[i]);
                bitstream += g2->encode(block_residuals_ch2[i]);
            }
        }
        if (g2) delete g2;
        

        mono_pred = temp_mono_pred;
        mid_pred = temp_mid_pred;
        side_pred = temp_side_pred;
    }

    // Escrita do bitstream comprimido em bytes no ficheiro de saída
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
    cout << "Codificação concluída (adaptativa por bloco): " << outputFile << endl;
    return 0;
}