#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include "utils.h"
#include "Golomb.h"

using namespace std;
namespace fs = std::filesystem;

// make bin/lossless_image_decoder
// ./bin/image_decoder out/compressed.gol out/decompressed.ppm

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " <entrada.gol> <saida.ppm>\n";
        return 1;
    }
    
    // --- Análise de argumentos e nomes de ficheiros ----
    string input = argv[1];
    string outArg = argv[2];
    string outputDir = "out/";
    fs::create_directories(outputDir);
    size_t pos = outArg.find_last_of("/\\");
    string filename = (pos != string::npos) ? outArg.substr(pos + 1) : outArg;
    if (filename.find(".ppm") == string::npos) filename += ".ppm";
    string outputPath = outputDir + filename;
    
    ifstream fin(input, ios::binary);
    if (!fin.is_open()) {
        cerr << "Erro: não foi possível abrir '" << input << "'\n";
        return 1;
    }

    // --- Leitura do cabeçalho ---
    char magic[5] = {0};
    fin.read(magic, 4);
    if (string(magic) != "GOL1") {
        cerr << "Erro: ficheiro inválido (magic)\n";
        return 1;
    }
    Image img;
    fin.read(reinterpret_cast<char*>(&img.width), sizeof(img.width));
    fin.read(reinterpret_cast<char*>(&img.height), sizeof(img.height));
    fin.read(reinterpret_cast<char*>(&img.maxval), sizeof(img.maxval));
    int channels = 1;
    if (!fin.read(reinterpret_cast<char*>(&channels), sizeof(channels))) channels = 1;

    // --- Leitura do bitBuffer ---
    string bitBuffer;
    unsigned char byte;
    while (fin.read(reinterpret_cast<char*>(&byte), 1)) {
        for (int i = 7; i >= 0; --i) bitBuffer += ((byte >> i) & 1) ? '1' : '0';
    }
    fin.close();

    img.channels = 1;
    img.data.assign(img.height, vector<int>(img.width, 0));
    
    size_t bitIndex = 0;
    const int blockSize = 16;
    cout << "Modo: 'm' adaptativo (lido por bloco).\n";

    try {
        for (int by = 0; by < img.height; by += blockSize) {
            for (int bx = 0; bx < img.width; bx += blockSize) {
                
                int mToUse = static_cast<int>(binary_string_to_int(bitBuffer, bitIndex, 16));
                if (mToUse <= 0) mToUse = 1;

                
                Golomb golomb(mToUse, SignHandling::INTERLEAVING);

                // Processa os píxeis dentro do bloco
                for (int y = by; y < min(by + blockSize, img.height); ++y) {
                    for (int x = bx; x < min(bx + blockSize, img.width); ++x) {
                        
                        int a = (x > 0) ? img.data[y][x-1] : 0;
                        int b = (y > 0) ? img.data[y-1][x] : 0;
                        int c = (x > 0 && y > 0) ? img.data[y-1][x-1] : 0;
                        int pred = predict(a,b,c);

                        int residual = golomb.decode(bitBuffer, bitIndex);
                        
                        int val = pred + residual;
                        if (val < 0) val = 0;
                        if (val > img.maxval) val = img.maxval;
                        img.data[y][x] = val;
                    }
                }
            }
        }
    } catch (const std::runtime_error& e) {
        cerr << "Erro fatal durante a descodificação: " << e.what() << endl;
        return 1;
    }
    
    if (!writeGrayAsPPM(outputPath, img)) {
        cerr << "Erro ao escrever imagem de saída\n";
        return 1;
    }

    cout << "Imagem descodificada e salva em '" << outputPath << "'\n";
    return 0;
}