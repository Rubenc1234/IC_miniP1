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
        cerr << "Uso: " << argv[0] << " <imagem.ppm> <saida.gol> [m]\n";
        cerr << "  Se [m] não for fornecido, calcula m óptimo por bloco.\n";
        return 1;
    }

    string input = argv[1];
    string outArg = argv[2];
    int m = -1;  // -1 significa: calcular m óptimo
    
    if (argc >= 4) {
        try {
            m = stoi(argv[3]);
            if (m <= 0) {
                cerr << "Erro: m deve ser um inteiro positivo.\n";
                return 1;
            }
            cerr << "Modo: usando m fixo = " << m << "\n";
        } catch (const invalid_argument& e) {
            cerr << "Erro: parâmetro 'm' deve ser um inteiro válido.\n";
            return 1;
        } catch (const out_of_range& e) {
            cerr << "Erro: parâmetro 'm' fora do intervalo.\n";
            return 1;
        }
    } else {
        cerr << "Modo: calculando m óptimo por bloco.\n";
    }

    string outputDir = "out/";
    fs::create_directories(outputDir);

    // extrai nome base
    size_t pos = outArg.find_last_of("/\\");
    string filename = (pos != string::npos) ? outArg.substr(pos + 1) : outArg;
    if (filename.find(".gol") == string::npos) filename += ".gol";
    string outputPath = outputDir + filename;
    cout << "Output path: " << outputPath << "\n";

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

    string bitBuffer;

    const int blockSize = 16; // 16x16 pixels
    for (int by = 0; by < img.height; by += blockSize) {
        for (int bx = 0; bx < img.width; bx += blockSize) {
            // 1. Recolher resíduos do bloco
            vector<int> blockResiduals;
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
            
            // 2. Determinar m a usar: fixo ou óptimo
            int mToUse = m;  // usa m fixo se foi fornecido (m != -1)
            if (m == -1) {
                // Se m não foi fornecido, calcula óptimo para este bloco
                mToUse = calculate_optimal_m(blockResiduals);
                printf("Bloco (%d,%d): m óptimo = %d\n", bx / blockSize, by / blockSize, mToUse);
            }
            
            // 3. Codificar o bloco com esse m
            Golomb golomb(mToUse, SignHandling::INTERLEAVING);
            for (int res : blockResiduals) {
                bitBuffer += golomb.encode(res);
            }
        }
    }

    // escrever bytes completos
    while (bitBuffer.size() >= 8) {
        unsigned char byte = 0;
        for (int i = 0; i < 8; ++i) {
            byte = (byte << 1) | (bitBuffer[i] - '0');
        }
        fout.write(reinterpret_cast<const char*>(&byte), 1);
        bitBuffer.erase(0,8);
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
