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

    int sampleRate, numChannels, m1, m2;
    sf_count_t numFrames;

    in.read((char*)&sampleRate, sizeof(int));
    in.read((char*)&numChannels, sizeof(int));
    in.read((char*)&numFrames, sizeof(sf_count_t));
    

    in.read((char*)&m1, sizeof(int));
    if (numChannels == 2) {
        in.read((char*)&m2, sizeof(int));
    }
    
    cout << "Descodificação (Canais=" << numChannels << "). M1=" << m1 << ", M2=" << m2 << endl;


    // Ler bits (Lógica original mantida)
    vector<char> fileBytes((istreambuf_iterator<char>(in)), {});
    in.close();

    string bitstream;
    bitstream.reserve(fileBytes.size() * 8);
    for (unsigned char b : fileBytes) {
        for (int i = 7; i >= 0; --i)
            bitstream += ((b >> i) & 1) ? '1' : '0';
    }

    // --- Descodificação e Reconstrução ---
    Golomb g1(m1, SignHandling::INTERLEAVING);
    Golomb g2(numChannels == 2 ? m2 : 1, SignHandling::INTERLEAVING);
    
    vector<int16_t> samples(numFrames * numChannels);
    size_t index = 0;

    if (numChannels == 1) {
        int16_t prediction = 0;
        for (sf_count_t i = 0; i < numFrames; i++) {
            int residual = g1.decode(bitstream, index);
            int reconstructed = prediction + residual;

            // Limitar ao intervalo de 16 bits
            if (reconstructed > 32767) reconstructed = 32767;
            if (reconstructed < -32768) reconstructed = -32768;

            samples[i] = static_cast<int16_t>(reconstructed);
            prediction = samples[i]; // O preditor simples
        }
    } else if (numChannels == 2) {
        int16_t mid_pred = 0;
        int16_t side_pred = 0;
        for (sf_count_t i = 0; i < numFrames; i++) {
            // Descodifica os resíduos de MID e SIDE
            int res_mid = g1.decode(bitstream, index);
            int res_side = g2.decode(bitstream, index);

            // Reconstrói os valores de MID e SIDE
            int16_t mid = static_cast<int16_t>(mid_pred + res_mid);
            int16_t side = static_cast<int16_t>(side_pred + res_side);

            int l_recon = mid + (side + 1) / 2;
            int r_recon = l_recon - side;

            // Limitar ao intervalo de 16 bits
            if (l_recon > 32767) l_recon = 32767;
            if (l_recon < -32768) l_recon = -32768;

            if (r_recon > 32767) r_recon = 32767;
            if (r_recon < -32768) r_recon = -32768;


            samples[i * 2] = static_cast<int16_t>(l_recon);
            samples[i * 2 + 1] = static_cast<int16_t>(r_recon);

            mid_pred = mid;
            side_pred = side;
        }
    }

    // Escrever WAV (Lógica original mantida)
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