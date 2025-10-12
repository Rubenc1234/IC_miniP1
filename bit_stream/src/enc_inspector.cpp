// enc_inspector.cpp
#include <iostream>
#include <sys/stat.h>
#include <cstdint>
#include <cmath>
using namespace std;

uint64_t file_size_bytes(const string &path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<uint64_t>(st.st_size);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <file.enc> <n_bits> <bs>\n";
        return 1;
    }
    string path = argv[1];
    int n_bits = stoi(argv[2]);
    int bs = stoi(argv[3]);
    double dctFrac = 0.2;
    uint64_t size = file_size_bytes(path);
    if (size == 0) { cerr << "Cannot stat file\n"; return 1; }
    uint64_t total_bits = size * 8ULL;
    uint64_t total_coeffs = total_bits / static_cast<uint64_t>(n_bits);
    size_t keep_sz = static_cast<size_t>(floor(bs * dctFrac));
    size_t nBlocks = static_cast<size_t>(total_coeffs / keep_sz);
    uint64_t numSamples = static_cast<uint64_t>(nBlocks) * bs;
    cout << "File bytes: " << size << "\n";
    cout << "Total bits: " << total_bits << "\n";
    cout << "Total coeffs (bits/" << n_bits << "): " << total_coeffs << "\n";
    cout << "bs=" << bs << " keep_sz=" << keep_sz << "\n";
    cout << "Inferred nBlocks=" << nBlocks << "\n";
    cout << "Inferred samples=" << numSamples << " (" << (numSamples/ (double)44100.0) << " s at 44.1kHz)\n";
    return 0;
}
