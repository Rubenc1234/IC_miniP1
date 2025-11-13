#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include "utils.h"
#include "Golomb.h"

using namespace std;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " <entrada.gol> <saida.ppm>\n";
        return 1;
    }

    string input = argv[1];
    string outArg = argv[2];

    string outputDir = "../out/";
    fs::create_directories(outputDir);

    // extrair nome base
    size_t pos = outArg.find_last_of("/\\");
    string filename = (pos != string::npos) ? outArg.substr(pos + 1) : outArg;
    if (filename.find(".ppm") == string::npos) filename += ".ppm";
    string outputPath = outputDir + filename;

    ifstream fin(input, ios::binary);
    if (!fin.is_open()) {
        cerr << "Erro: não foi possível abrir '" << input << "'\n";
        return 1;
    }

    // ler cabeçalho
    char magic[5] = {0};
    fin.read(magic, 4);
    if (string(magic) != "GOL1") {
        cerr << "Erro: ficheiro inválido (magic)\n";
        return 1;
    }

    // ler largura, altura, maxval, m, channels
    Image img;
    fin.read(reinterpret_cast<char*>(&img.width), sizeof(img.width));
    fin.read(reinterpret_cast<char*>(&img.height), sizeof(img.height));
    fin.read(reinterpret_cast<char*>(&img.maxval), sizeof(img.maxval));
    int m = 0;
    fin.read(reinterpret_cast<char*>(&m), sizeof(m));
    int channels = 1;
    if (!fin.read(reinterpret_cast<char*>(&channels), sizeof(channels))) channels = 1; // compat

    // ler dados do ficheiro para bitBuffer
    string bitBuffer;
    unsigned char byte;
    while (fin.read(reinterpret_cast<char*>(&byte), 1)) {
        for (int i = 7; i >= 0; --i) bitBuffer += ((byte >> i) & 1) ? '1' : '0';
    }
    fin.close();

    // prepara imagem grayscale
    img.channels = 1;
    img.data.assign(img.height, vector<int>(img.width, 0));

    // decodificador Golomb
    Golomb golomb(m, SignHandling::INTERLEAVING);
    size_t bitIndex = 0;

    // percorre, calcula preditor e decodifica resíduos
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
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

    if (!writeGrayAsPPM(outputPath, img)) {
        cerr << "Erro ao escrever imagem de saída\n";
        return 1;
    }

    cout << "Imagem descodificada e salva em '" << outputPath << "'\n";
    return 0;
}
