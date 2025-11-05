#include "utils.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <cmath>

using namespace std;

int predict(int a, int b, int c) {
    if (c >= max(a, b)) return min(a, b);
    else if (c <= min(a, b)) return max(a, b);
    else return a + b - c;
}

int residualToUnsigned(int r) {
    return (r >= 0) ? (2 * r) : (-2 * r - 1);
}

int unsignedToResidual(int n) {
    return ( (n & 1) == 0 ) ? (n / 2) : ( - ( (n + 1) / 2 ) );
}

bool readPPMtoGray(const string& filename, Image& img) {

    // abrir ficheiro
    ifstream in(filename, ios::binary);
    if (!in.is_open()) {
        cerr << "Erro: não foi possível abrir " << filename << endl;
        return false;
    }

    // ler cabeçalho PPM
    string magic;
    in >> magic;
    if (magic != "P6") {
        cerr << "Erro: só está suportado PPM binário (P6). Encontrado: " << magic << endl;
        return false;
    }

    // Ignorar comentários
    while (in.peek() == '#') in.ignore(numeric_limits<streamsize>::max(), '\n');

    // ler largura, altura, maxval
    in >> img.width >> img.height >> img.maxval;
    in.get(); // consumir whitespace depois do cabeçalho

    if (img.width <= 0 || img.height <= 0) {    
        cerr << "Erro: dimensões inválidas no ficheiro PPM\n";
        return false;
    }
    if (img.maxval <= 0 || img.maxval > 255) {
        cerr << "Aviso: maxval fora de 1..255 — será tratado como 255\n";
        img.maxval = 255;
    }

    img.channels = 1; // grayscale
    img.data.assign(img.height, vector<int>(img.width, 0)); // alocar espaço

    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            unsigned char rgb[3]; // ler RGB
            in.read(reinterpret_cast<char*>(rgb), 3); // cada canal 1 byte
            if (!in) {
                cerr << "Erro: leitura PPM truncada\n";
                return false;
            }
            // converter para luminância (BT.601-like): Y = 0.299 R + 0.587 G + 0.114 B
            double yval = 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2];
            int iv = static_cast<int>(round(yval)); // arredondar

            // garantir dentro de [0, maxval]
            if (iv < 0) iv = 0; 
            if (iv > img.maxval) iv = img.maxval;

            // armazenar valor grayscale
            img.data[y][x] = iv;
        }
    }

    in.close();
    return true;
}

bool writeGrayAsPPM(const string& filename, const Image& img) {
    if (img.width <= 0 || img.height <= 0 || img.data.empty()) {
        cerr << "Erro: imagem inválida para escrita\n";
        return false;
    }

    ofstream out(filename, ios::binary);
    if (!out.is_open()) {
        cerr << "Erro: não foi possível criar " << filename << endl;
        return false;
    }

    // escrever cabeçalho PPM
    out << "P6\n" << img.width << " " << img.height << "\n" << img.maxval << "\n";

    // escrever dados da imagem
    for (int y = 0; y < img.height; ++y) {
        for (int x = 0; x < img.width; ++x) {
            unsigned char v = static_cast<unsigned char>(img.data[y][x]);
            out.write(reinterpret_cast<const char*>(&v), 1);
            out.write(reinterpret_cast<const char*>(&v), 1);
            out.write(reinterpret_cast<const char*>(&v), 1);
        }
    }

    out.close();
    return true;
}
