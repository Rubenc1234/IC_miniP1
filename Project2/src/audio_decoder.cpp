#include <iostream>
#include <vector>
#include <sndfile.h>
#include "Golomb.h"
#include <fstream>
#include <string> 
#include <stdexcept> 
#include "utils.h" 

using namespace std;

// Função principal para descodificar ficheiro binário comprimido em WAV
int main(int argc, char* argv[]) {
    // Verificação dos argumentos de linha de comando
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " <input.bin> <output.wav>\n";
        return 1;
    }

    const char* inputFile = argv[1];
    const char* outputFile = argv[2];

    // Leitura dos metadados do ficheiro binário de entrada
    ifstream in(inputFile, ios::binary);
    if (!in.is_open()) {
        cerr << "Erro ao abrir ficheiro: " << inputFile << endl;
        return 1;
    }

    int sampleRate, numChannels;
    sf_count_t numFrames;

    in.read((char*)&sampleRate, sizeof(int));
    in.read((char*)&numChannels, sizeof(int));
    in.read((char*)&numFrames, sizeof(sf_count_t));
    
    cout << "Descodificação (Canais=" << numChannels << ", m adaptativo por bloco)\n";

    // Conversão do resto do ficheiro em bitstream
    vector<char> fileBytes((istreambuf_iterator<char>(in)), {});
    in.close();

    string bitstream;
    bitstream.reserve(fileBytes.size() * 8);
    for (unsigned char b : fileBytes) {
        for (int i = 7; i >= 0; --i)
            bitstream += ((b >> i) & 1) ? '1' : '0';
    }

    
    vector<int16_t> samples(numFrames * numChannels);
    size_t index = 0; 
    const int blockSize = 4096; 


    int16_t mono_pred = 0;
    int16_t mid_pred = 0;
    int16_t side_pred = 0;

    // Processamento por blocos para descodificação adaptativa
    try {
        for (sf_count_t frame_start = 0; frame_start < numFrames; frame_start += blockSize) {
            sf_count_t frame_end = min(frame_start + blockSize, numFrames);
            sf_count_t framesInBlock = frame_end - frame_start;
            

            // Leitura dos parâmetros m para o bloco
            int m1 = static_cast<int>(binary_string_to_int(bitstream, index, 16));
            if (m1 <= 0) m1 = 1;
            int m2 = 0;
            if (numChannels == 2) {
                m2 = static_cast<int>(binary_string_to_int(bitstream, index, 16));
                if (m2 <= 0) m2 = 1;
            }

            Golomb g1(m1, SignHandling::INTERLEAVING);
            Golomb g2(m2 > 0 ? m2 : 1, SignHandling::INTERLEAVING);

            // Descodificação e reconstrução das amostras por bloco
            for (sf_count_t i = 0; i < framesInBlock; i++) {
                sf_count_t frame_index = frame_start + i; // Índice global da trama

                if (numChannels == 1) {
                    // Reconstrução temporal para mono: soma do resíduo ao preditor
                    int prediction = mono_pred;

                    int residual = g1.decode(bitstream, index);
                    int reconstructed = prediction + residual;
                    
                    if (reconstructed > 32767) reconstructed = 32767;
                    if (reconstructed < -32768) reconstructed = -32768;

                    samples[frame_index] = static_cast<int16_t>(reconstructed);
                    mono_pred = samples[frame_index]; 
                
                } else { 
                    // Reconstrução inter-canal para estéreo: cálculo de L e R a partir de mid e side
                    int pred_mid = mid_pred;
                    int pred_side = side_pred;

                    int res_mid = g1.decode(bitstream, index);
                    int res_side = g2.decode(bitstream, index);

                    int16_t mid = static_cast<int16_t>(pred_mid + res_mid);
                    int16_t side = static_cast<int16_t>(pred_side + res_side);

   
                    int l_recon = mid + (side + 1) / 2;
                    int r_recon = l_recon - side;

                    // Clamping para evitar overflow
                    if (l_recon > 32767) {
                        l_recon = 32767;
                    } else if (l_recon < -32768) { 
                        l_recon = -32768;
                    }

                    if (r_recon > 32767) {
                        r_recon = 32767;
                    } else if (r_recon < -32768) {
                        r_recon = -32768;
                    }

                    samples[frame_index * 2] = static_cast<int16_t>(l_recon);
                    samples[frame_index * 2 + 1] = static_cast<int16_t>(r_recon);

                    mid_pred = mid;
                    side_pred = side;
                }
            }
        }
    } catch (const std::runtime_error& e) {
        cerr << "Erro fatal durante a descodificação: " << e.what() << endl;
        return 1;
    }

    // Escrita das amostras reconstruídas no ficheiro WAV de saída
    SF_INFO sfInfoOut;
    sfInfoOut.samplerate = sampleRate;
    sfInfoOut.channels = numChannels;
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