// wav_quant_dec_fixed.cpp
#include "bit_stream.h"
#include <fftw3.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sys/stat.h>
#include <algorithm>

using namespace std;

uint64_t file_size_bytes(const string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<uint64_t>(st.st_size);
}

void write_little_endian(ofstream &ofs, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        uint8_t b = (value >> (8 * i)) & 0xFF;
        ofs.put(static_cast<char>(b));
    }
}

int main(int argc, char* argv[]) {
    if (argc < 6) {
        cerr << "Usage: " << argv[0] << " <input.enc> <output.wav> <n_bits> <channels(=1)> <sample_rate> [orig_bits]\n";
        return 1;
    }
    

    string in_path  = argv[1];
    string out_path = argv[2];
    int n_bits = stoi(argv[3]);
    int channels = stoi(argv[4]); // expect 1
    int sample_rate = stoi(argv[5]);
    int orig_bits = 16;
    if (argc >= 7) orig_bits = stoi(argv[6]);

    const uint64_t q_levels = (1ULL << n_bits);
    uint64_t coeffs_read = 0;
    const size_t SHOW_N = 64;
    vector<uint64_t> first_qs;
    first_qs.reserve(SHOW_N);

    fstream ifs_enc { in_path, ios::in | ios::binary };
    if (!ifs_enc.is_open()) {
        cerr << "Error opening encoded file: " << in_path << "\n";
        return 1;
    }

    // --- HEADER LEITURA ---
    uint32_t magic;
    uint16_t header_bs, header_keep;
    
    ifs_enc.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    ifs_enc.read(reinterpret_cast<char*>(&header_bs), sizeof(header_bs));
    ifs_enc.read(reinterpret_cast<char*>(&header_keep), sizeof(header_keep));
    
    if (magic != 0x44435431) {
        cerr << "Formato inválido ou ficheiro corrompido (magic != DCT1)\n";
        return 1;
    }
    
    size_t bs = header_bs;
    size_t keep_sz = header_keep;
    
    cout << "Header lido: bs=" << bs << " keep_sz=" << keep_sz
         << " (frac = " << (double)keep_sz / bs << ")\n";

    if (channels != 1) {
        cerr << "This decoder expects mono (channels==1)\n";
        return 1;
    }

    // Preparacao para leitura dos dados codificados

    uint64_t in_size = file_size_bytes(in_path);
    if (in_size == 0) {
        cerr << "Cannot stat input file or file empty: " << in_path << "\n";
        return 1;
    }
    cout << "Encoded file bytes: " << in_size << "\n";

    const uint64_t total_bits = in_size * 8ULL;
    const uint64_t total_coeffs = total_bits / static_cast<uint64_t>(n_bits);
    if (keep_sz < 1) keep_sz = 1;

    if (total_coeffs < keep_sz) {
        cerr << "Not enough coefficients in file\n";
        return 1;
    }

    // nBlocks inferred (must corresponder ao encoder)
    size_t nBlocks = static_cast<size_t>(total_coeffs / keep_sz);
    if (nBlocks == 0) { cerr << "No blocks inferred\n"; return 1; }
    uint64_t numSamples = static_cast<uint64_t>(nBlocks) * bs;

    cout << "Inferred keep_sz=" << keep_sz << " nBlocks=" << nBlocks << " numSamples=" << numSamples << "\n";
    
    BitStream ibs(ifs_enc, STREAM_READ);

    ofstream ofs(out_path, ios::out | ios::binary);
    if (!ofs.is_open()) {
        cerr << "Error opening output WAV: " << out_path << "\n";
        return 1;
    }

    // Write simple WAV header (PCM)

    uint16_t audio_format = 1; // PCM
    uint16_t num_channels = static_cast<uint16_t>(channels);
    uint32_t sample_rate_u = static_cast<uint32_t>(sample_rate);
    uint16_t bits_per_sample = static_cast<uint16_t>(orig_bits);
    uint16_t block_align = num_channels * (bits_per_sample / 8);
    uint32_t byte_rate = sample_rate_u * block_align;
    uint32_t subchunk2_size = static_cast<uint32_t>(numSamples * block_align);
    uint32_t chunk_size = 36 + subchunk2_size;

    ofs.write("RIFF", 4);
    write_little_endian(ofs, chunk_size, 4);
    ofs.write("WAVE", 4);
    ofs.write("fmt ", 4);
    write_little_endian(ofs, 16, 4); // subchunk1 size PCM
    write_little_endian(ofs, audio_format, 2);
    write_little_endian(ofs, num_channels, 2);
    write_little_endian(ofs, sample_rate_u, 4);
    write_little_endian(ofs, byte_rate, 4);
    write_little_endian(ofs, block_align, 2);
    write_little_endian(ofs, bits_per_sample, 2);
    ofs.write("data", 4);
    write_little_endian(ofs, subchunk2_size, 4);

    vector<double> X(bs, 0.0), x(bs, 0.0);
    fftw_plan plan_i = fftw_plan_r2r_1d(static_cast<int>(bs), X.data(), x.data(), FFTW_REDFT01, FFTW_ESTIMATE);
    if (!plan_i) {
        cerr << "FFTW plan creation failed\n";
        return 1;
    }



    // Decode: iterate TODOS os blocos (sem /2)
    for (size_t b = 0; b < nBlocks; ++b) {
        for (size_t k = 0; k < keep_sz; ++k) {

            // read quantized value

            uint64_t q = ibs.read_n_bits(n_bits);

            // dequantize
            if (q >= q_levels) q = q_levels - 1;
            X[k] = (static_cast<double>(q) / (q_levels-1) * 2.0 - 1.0) * static_cast<double>(bs);
                    
            coeffs_read++;
            if (first_qs.size() < SHOW_N) first_qs.push_back(q);
                    
        }

        // zero remaining coefficients
        for (size_t k = keep_sz; k < bs; ++k) X[k] = 0.0;


        // IDCT
        fftw_execute(plan_i);

        /*
        if (b == 0) {
            ofstream csv("reconstructed_block.csv");
            for (size_t n = 0; n < bs; ++n)
                csv << x[n] << "\n";
            csv.close();
        }
            */


        // Escala para int16
        for (size_t n = 0; n < bs; ++n) {
            // Normaliza para [-1.0, +1.0]
            double sample_val = (x[n] / (2.0 * bs)) * 32768.0;
            if (sample_val > 32767) sample_val = 32767;
            if (sample_val < -32768) sample_val = -32768;

            // Arredonda para o inteiro mais próximo
            int16_t out_s = static_cast<int16_t>(lround(sample_val));
            ofs.write(reinterpret_cast<char*>(&out_s), sizeof(out_s));
        }

        
    }

    // print debug info
    cout << "DEBUG: coeffs_read = " << coeffs_read << "\n";
    cout << "DEBUG: first " << first_qs.size() << " q values read:\n";
    for (size_t i = 0; i < first_qs.size(); ++i) {
        cout << first_qs[i] << ( (i+1)%8==0 ? "\n" : " " );
    }
    if (first_qs.size() % 8 != 0) cout << "\n";


    fftw_destroy_plan(plan_i);
    ibs.close();
    ifs_enc.close();
    ofs.close();

    cout << "Decoded WAV written to: " << out_path << "  (frames = " << numSamples << ")\n";
    return 0;
}
