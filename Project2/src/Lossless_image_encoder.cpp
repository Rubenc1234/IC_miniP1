#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "utils.h"
#include "Golomb.h"

using namespace std;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 4) {
        cerr << "Uso: " << argv[0] << " <imagem.ppm> <saida.gol> <m>\n";
        return 1;
    }

    string input = argv[1];
    string outArg = argv[2];
    int m = stoi(argv[3]);

    string outputDir = "../out/";
    fs::create_directories(outputDir);

    // extrai nome base
    size_t pos = outArg.find_last_of("/\\");
    string filename = (pos != string::npos) ? outArg.substr(pos + 1) : outArg;
    if (filename.find(".gol") == string::npos) filename += ".gol";
    string outputPath = outputDir + filename;

    // ler e converter imagem para grayscale
    Image img;
    if (!readPPMtoGray(input, img)) {
        cerr << "Erro: falha a ler/convert PPM -> grayscale\n";
        return 1;
    }

    ofstream fout(outputPath, ios::binary);
    if (!fout.is_open()) {
        cerr << "Erro: não foi possível criar '" << outputPath << "'\n";
        return 1;
    }

    // Cabeçalho: magic + largura + altura + maxval + m + channels(=1)
    fout.write("GOL1", 4);
    fout.write(reinterpret_cast<const char*>(&img.width), sizeof(img.width));
    fout.write(reinterpret_cast<const char*>(&img.height), sizeof(img.height));
    fout.write(reinterpret_cast<const char*>(&img.maxval), sizeof(img.maxval));
    fout.write(reinterpret_cast<const char*>(&m), sizeof(m));
    int channels = 1;
    fout.write(reinterpret_cast<const char*>(&channels), sizeof(channels));

    // codificador Golomb
    Golomb golomb(m, SignHandling::INTERLEAVING);
    string bitBuffer;

    // percorre, calcula preditor e codifica resíduos
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            int a = (x > 0) ? img.data[y][x-1] : 0;
            int b = (y > 0) ? img.data[y-1][x] : 0;
            int c = (x > 0 && y > 0) ? img.data[y-1][x-1] : 0;
            int pred = predict(a,b,c);
            int residual = img.data[y][x] - pred;
            int mapped = residualToUnsigned(residual);

            string code = golomb.encode(mapped);
            bitBuffer += code;

            // escrever bytes completos
            while (bitBuffer.size() >= 8) {
                unsigned char byte = 0;
                for (int i = 0; i < 8; ++i) {
                    byte = (byte << 1) | (bitBuffer[i] - '0');
                }
                fout.write(reinterpret_cast<const char*>(&byte), 1);
                bitBuffer.erase(0,8);
            }
        }
    }

    // padding dos bits remanescentes (à direita com zeros)
    if (!bitBuffer.empty()) {
        unsigned char byte = 0;
        for (size_t i = 0; i < bitBuffer.size(); ++i) byte = (byte << 1) | (bitBuffer[i] - '0');
        byte <<= (8 - bitBuffer.size());
        fout.write(reinterpret_cast<const char*>(&byte), 1);
    }

    fout.close();
    cout << "Imagem codificada em grayscale e escrita em '" << outputPath << "'\n";
    return 0;
}
