#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>
#include <stdexcept> // Para std::invalid_argument e std::out_of_range
#include <algorithm> // Para std::max e std::min
#include <cmath>     // Para std::round

using namespace cv;
using namespace std;

int main(int argc, char** argv) {
    // Verifica se o número correto de argumentos foi fornecido.
    if (argc != 4) {
        cerr << "Uso: " << argv[0] << " <imagem_entrada> <imagem_saida> <percentagem_ajuste>\n";
        cerr << "     percentagem_ajuste: inteiro entre -100 e 100 (0 = sem alteração).\n"; // Mensagem atualizada
        return -1;
    }

    string inputFile = argv[1];
    string outputFile = argv[2];
    int percentageValue = 0;

    // Converte o valor percentual de string para inteiro, com tratamento de erros.
    try {
        percentageValue = stoi(argv[3]);
    } catch (const invalid_argument& e) {
        cerr << "Erro: Valor percentual inválido. Deve ser um número inteiro.\n";
        return -1;
    } catch (const out_of_range& e) {
        cerr << "Erro: Valor percentual fora do intervalo permitido [-100, 100].\n";
        return -1;
    }

    // --- NOVA VALIDAÇÃO ---
    // Verifica se o valor percentual está no intervalo [-100, 100]
    if (percentageValue < -100 || percentageValue > 100) {
        cerr << "Erro: Valor percentual fora do intervalo permitido [-100, 100].\n";
        return -1;
    }
    // --- FIM DA NOVA VALIDAÇÃO ---

    // --- MAPEAMENTO PERCENTAGEM -> VALOR ADITIVO ---
    // Mapeia a percentagem [-100, 100] para o valor aditivo [-255, 255]
    int adjustmentValue = static_cast<int>(round(percentageValue * 2.55));
    // --- FIM DO MAPEAMENTO ---


    // Lê a imagem de entrada a cores.
    Mat img = imread(inputFile, IMREAD_COLOR);
    if (img.empty()) {
        cerr << "Erro: Não foi possível abrir a imagem '" << inputFile << "'.\n";
        return -1;
    }

    // Cria uma matriz de saída com as mesmas dimensões e tipo da original.
    Mat adjustedImage = Mat::zeros(img.size(), img.type());

    cout << "A ajustar a intensidade em " << percentageValue << "% (valor aditivo: " << adjustmentValue << ")...\n"; // Mensagem atualizada

    // Itera sobre cada píxel da imagem.
    for (int r = 0; r < img.rows; ++r) {
        for (int c = 0; c < img.cols; ++c) {
            // Obtém o píxel original (B, G, R).
            Vec3b originalPixel = img.at<Vec3b>(r, c);
            Vec3b adjustedPixel;

            // Ajusta cada canal de cor (B, G, R).
            for (int channel = 0; channel < 3; ++channel) {
                // Calcula o novo valor somando o ajuste MAPEADO.
                int newValue = originalPixel[channel] + adjustmentValue;

                // Garante (clamp) que o valor permanece no intervalo [0, 255].
                adjustedPixel[channel] = saturate_cast<uchar>(newValue);
            }
            // Atribui o píxel ajustado à imagem de saída.
            adjustedImage.at<Vec3b>(r, c) = adjustedPixel;
        }
    }

    // Tenta guardar a imagem com intensidade ajustada.
    if (imwrite(outputFile, adjustedImage)) {
        cout << "Imagem com intensidade ajustada guardada com sucesso em '" << outputFile << "'\n";
    } else {
        cerr << "Erro: Não foi possível guardar a imagem em '" << outputFile << "'.\n";
        return -1;
    }

    return 0;
}