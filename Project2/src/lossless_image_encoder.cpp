#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector> // Necessário para vector
#include <cmath>  // Necessário para round, log2
#include "utils.h"
#include "Golomb.h"

using namespace std;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc != 3) {
        cerr << "Uso: " << argv[0] << " <imagem.ppm> <saida.gol>\n";
        cerr << "     (O 'm' ótimo é calculado automaticamente por bloco)\n";
        return 1;
    }

    string input = argv[1];
    string outArg = argv[2];

    string outputDir = "out/";
    fs::create_directories(outputDir);
    
    
    size_t pos = outArg.find_last_of("/\\");
    string filename = (pos != string::npos) ? outArg.substr(pos + 1) : outArg;
    if (filename.find(".gol") == string::npos) filename += ".gol";
    string outputPath = outputDir + filename;

    Image img;
    if (!readPPMtoGray(input, img)) {
        cerr << "Erro: falha a ler/converter PPM -> grayscale\n";
        return 1;
    }

    ofstream fout(outputPath, ios::binary);
    // --- Escrita do cabeçalho) ---
    fout.write("GOL1", 4);
    fout.write(reinterpret_cast<const char*>(&img.width), sizeof(img.width));
    fout.write(reinterpret_cast<const char*>(&img.height), sizeof(img.height));
    fout.write(reinterpret_cast<const char*>(&img.maxval), sizeof(img.maxval));
    int channels = 1;
    fout.write(reinterpret_cast<const char*>(&channels), sizeof(channels));

    string bitBuffer;
    const int blockSize = 16; 

    for (int by = 0; by < img.height; by += blockSize) {
        for (int bx = 0; bx < img.width; bx += blockSize) {
            
            vector<int> blockResiduals;
            // 1. Calcular resíduos do bloco
            for (int y = by; y < min(by + blockSize, img.height); ++y) {
                for (int x = bx; x < min(bx + blockSize, img.width); ++x) {
                    int a = (x > 0) ? img.data[y][x-1] : 0;
                    int b = (y > 0) ? img.data[y-1][x] : 0;
                    int c = (x > 0 && y > 0) ? img.data[y-1][x-1] : 0;
                    int pred = predict(a, b, c);
                    int residual = img.data[y][x] - pred;
                    blockResiduals.push_back(residual);
                }
            }
            
            // 2. Calcular 'm' ótimo para este bloco
            int mToUse = calculate_optimal_m(blockResiduals);
            // printf("Bloco (%d,%d): m óptimo = %d\n", bx / blockSize, by / blockSize, mToUse);
            
            bitBuffer += int_to_binary_string(mToUse, 16); 
            
            // 3. Codificar o bloco com o 'm' calculado
            Golomb golomb(mToUse, SignHandling::INTERLEAVING);
            for (int res : blockResiduals) {
                bitBuffer += golomb.encode(res);
            }
        }
    }

    // 4. Escrever o bitstream para o ficheiro
    while (bitBuffer.size() >= 8) {
        string byteStr = bitBuffer.substr(0, 8);
        uint8_t byte = static_cast<uint8_t>(stoi(byteStr, nullptr, 2));
        fout.write(reinterpret_cast<const char*>(&byte), 1);
        bitBuffer.erase(0,8);
    }
    if (!bitBuffer.empty()) {
        bitBuffer.append(8 - bitBuffer.size(), '0'); // Padding
        uint8_t byte = static_cast<uint8_t>(stoi(bitBuffer, nullptr, 2));
        fout.write(reinterpret_cast<const char*>(&byte), 1);
    }

    fout.close();
    cout << "Imagem codificada (m adaptativo) e escrita em '" << outputPath << "'\n";
    return 0;
}