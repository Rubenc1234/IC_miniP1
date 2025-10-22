#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <stdexcept> // Para std::invalid_argument e std::out_of_range

using namespace cv;
using namespace std;

int main(int argc, char** argv) {
    // Verifica se o número correto de argumentos foi fornecido.
    if (argc != 4) {
        cerr << "Uso: " << argv[0] << " <imagem_entrada> <imagem_saida> <angulo>\n";
        cerr << "     angulo: Qualquer múltiplo de 90 (positivo, negativo ou zero).\n";
        return -1;
    }

    string inputFile = argv[1];
    string outputFile = argv[2];
    int angle = 0;

    // Converte o ângulo de string para inteiro, com tratamento de erros.
    try {
        angle = stoi(argv[3]);
    } catch (const invalid_argument& e) {
        cerr << "Erro: Ângulo inválido. Deve ser um número inteiro.\n";
        return -1;
    } catch (const out_of_range& e) {
        cerr << "Erro: Ângulo fora do intervalo permitido.\n";
        return -1;
    }

    // --- ALTERAÇÃO: Validação Generalizada ---
    // Valida se o ângulo é um múltiplo de 90.
    if (angle % 90 != 0) {
        cerr << "Erro: Ângulo inválido. Deve ser um múltiplo de 90 (e.g., -180, 0, 90, 270).\n";
        return -1;
    }

    // --- ALTERAÇÃO: Normalização do Ângulo ---
    // Normaliza o ângulo para um valor equivalente em [0, 90, 180, 270] graus no sentido horário.
    // Ex: -90 -> 270, 360 -> 0, 450 -> 90
    int normalized_angle = (angle % 360 + 360) % 360;

    // Lê a imagem de entrada a cores.
    Mat img = imread(inputFile, IMREAD_COLOR);
    if (img.empty()) {
        cerr << "Erro: Não foi possível abrir a imagem '" << inputFile << "'.\n";
        return -1;
    }

    int inRows = img.rows;
    int inCols = img.cols;
    Mat rotatedImage; // Matriz para a imagem rodada.

    cout << "A rodar a imagem " << angle << " graus (equivalente a " << normalized_angle << " graus horário)...\n";

    // --- ALTERAÇÃO: Lógica com base no ângulo normalizado ---
    // Determina o tamanho da imagem de saída e cria a matriz.
    if (normalized_angle == 90 || normalized_angle == 270) {
        rotatedImage.create(inCols, inRows, img.type());
    } else { // normalized_angle == 0 || normalized_angle == 180
        rotatedImage.create(inRows, inCols, img.type());
    }

    // --- ALTERAÇÃO: Adicionado caso para 0 graus ---
    if (normalized_angle == 0) {
        // Se o ângulo for 0 (ou múltiplo de 360), a imagem rodada é uma cópia da original.
        img.copyTo(rotatedImage);
    } else {
        // Itera sobre os píxeis da imagem DE SAÍDA para calcular a sua origem.
        for (int r = 0; r < rotatedImage.rows; ++r) {
            for (int c = 0; c < rotatedImage.cols; ++c) {
                Vec3b sourcePixel;
                // Calcula as coordenadas do píxel de origem correspondente, baseado no ângulo NORMALIZADO.
                switch (normalized_angle) {
                    case 90:
                        sourcePixel = img.at<Vec3b>(inRows - 1 - c, r);
                        break;
                    case 180:
                        sourcePixel = img.at<Vec3b>(inRows - 1 - r, inCols - 1 - c);
                        break;
                    case 270:
                        sourcePixel = img.at<Vec3b>(c, inCols - 1 - r);
                        break;
                    // O caso 0 já foi tratado.
                }
                // Atribui a cor do píxel de origem ao píxel de destino.
                rotatedImage.at<Vec3b>(r, c) = sourcePixel;
            }
        }
    }

    // Tenta guardar a imagem rodada.
    if (imwrite(outputFile, rotatedImage)) {
        cout << "Imagem rodada guardada com sucesso em '" << outputFile << "'\n";
    } else {
        cerr << "Erro: Não foi possível guardar a imagem em '" << outputFile << "'.\n";
        return -1;
    }

    return 0;
}