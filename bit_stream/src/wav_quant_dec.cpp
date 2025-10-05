// wav_quant_dec.cpp
#include <iostream>
#include <fstream>
#include <cstdint>
#include <cstring>
#include <sys/stat.h>
#include "bit_stream.h"

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
        cerr << "Usage: " << argv[0] << " <input.enc> <output.wav> <n_bits> <channels> <sample_rate> [orig_bits]\n";
        return 1;
    }

    string in_path  = argv[1];
    string out_path = argv[2];
    int n_bits = stoi(argv[3]);            // bits stored per sample in .enc
    int channels = stoi(argv[4]);          // 1 or 2
    int sample_rate = stoi(argv[5]);       // e.g. 44100
    int orig_bits = 16;
    if (argc >= 7) orig_bits = stoi(argv[6]);

    if (n_bits <= 0 || n_bits > 32) {
        cerr << "n_bits must be between 1 and 32\n";
        return 1;
    }
    if (orig_bits % 8 != 0) {
        cerr << "orig_bits must be a multiple of 8 (e.g., 16)\n";
        return 1;
    }

    uint64_t in_size = file_size_bytes(in_path);
    if (in_size == 0) {
        cerr << "Cannot stat input file or file empty: " << in_path << "\n";
        return 1;
    }

    // Calculate number of frames:
    // total_bits_in_file = in_size * 8
    // bits_per_frame = n_bits * channels
    uint64_t total_bits = in_size * 8ULL;
    uint64_t bits_per_frame = static_cast<uint64_t>(n_bits) * static_cast<uint64_t>(channels);
    if (bits_per_frame == 0) {
        cerr << "Invalid bits_per_frame\n";
        return 1;
    }
    uint64_t frames = total_bits / bits_per_frame;
    if (frames == 0) {
        cerr << "No frames computed from input size (maybe incorrect n_bits/channels?)\n";
        return 1;
    }

    cout << "Input file bytes: " << in_size << "\n";
    cout << "n_bits: " << n_bits << " channels: " << channels << " sample_rate: " << sample_rate << " orig_bits: " << orig_bits << "\n";
    cout << "Estimated frames: " << frames << "\n";

    // Open input .enc using fstream and BitStream
    fstream ifs_enc { in_path, ios::in | ios::binary };
    if (!ifs_enc.is_open()) {
        cerr << "Error opening encoded file: " << in_path << "\n";
        return 1;
    }
    BitStream ibs(ifs_enc, STREAM_READ);

    // Open output WAV (binary)
    ofstream ofs(out_path, ios::out | ios::binary);
    if (!ofs.is_open()) {
        cerr << "Error opening output WAV: " << out_path << "\n";
        return 1;
    }

    // Prepare WAV header values
    uint16_t audio_format = 1; // PCM
    uint16_t num_channels = static_cast<uint16_t>(channels);
    uint32_t sample_rate_u = static_cast<uint32_t>(sample_rate);
    uint16_t bits_per_sample = static_cast<uint16_t>(orig_bits);
    uint16_t block_align = num_channels * (bits_per_sample / 8);
    uint32_t byte_rate = sample_rate_u * block_align;
    uint32_t subchunk2_size = static_cast<uint32_t>(frames * block_align);
    uint32_t chunk_size = 36 + subchunk2_size;

    // Write WAV header (RIFF)
    ofs.write("RIFF", 4);
    write_little_endian(ofs, chunk_size, 4);
    ofs.write("WAVE", 4);

    // fmt subchunk
    ofs.write("fmt ", 4);
    write_little_endian(ofs, 16, 4); // subchunk1 size PCM
    write_little_endian(ofs, audio_format, 2);
    write_little_endian(ofs, num_channels, 2);
    write_little_endian(ofs, sample_rate_u, 4);
    write_little_endian(ofs, byte_rate, 4);
    write_little_endian(ofs, block_align, 2);
    write_little_endian(ofs, bits_per_sample, 2);

    // data subchunk
    ofs.write("data", 4);
    write_little_endian(ofs, subchunk2_size, 4);

    // For each frame, read channels x n_bits and expand to orig_bits and write
    const int bytes_per_sample = bits_per_sample / 8;
    const int shift = orig_bits - n_bits; // left shift to expand
    // We'll write signed PCM if orig_bits == 16; we reconstruct as integer then cast to int16_t
    for (uint64_t f = 0; f < frames; ++f) {
        for (int ch = 0; ch < channels; ++ch) {
            uint64_t q = ibs.read_n_bits(n_bits); // quantized value (0 .. 2^n_bits-1)
            // Expand
            uint64_t expanded = (q << shift);

            // If original was signed 16-bit two's complement, we need to map expanded (unsigned) to signed range.
            // This mapping depends on how encoder performed quantization. Here we assume encoder simply right-shifted
            // the raw 16-bit value (unsigned reinterpretation). We reconstruct by left-shifting back and casting.
            // Write little-endian bytes of size bytes_per_sample
            for (int b = 0; b < bytes_per_sample; ++b) {
                uint8_t outb = static_cast<uint8_t>((expanded >> (8 * b)) & 0xFF);
                ofs.put(static_cast<char>(outb));
            }
        }
    }

    // Close streams
    ibs.close();
    ifs_enc.close();
    ofs.close();

    cout << "Decoded WAV written to: " << out_path << "\n";
    return 0;
}
