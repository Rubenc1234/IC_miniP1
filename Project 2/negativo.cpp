#include <opencv2/opencv.hpp>
#include <iostream>

/*
g++ negativo.cpp -o negativo `pkg-config --cflags --libs opencv4`
./negativo imagens\ PPM/airplane.ppm negative.jpg view
*/

int main(int argc, char** argv) {
    if (argc < 3) {    
        std::cerr << "Uso: " << argv[0] << " <arquivo_entrada> <arquivo_saida> [view]" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    bool viewImage = (argc >= 4 && std::string(argv[3]) == "view");

    // cv::Mat -> imagem como matriz
    cv::Mat img = cv::imread(inputFile);
    if (img.empty()) {
        std::cerr << "Não foi possível abrir a imagem: " << inputFile << std::endl;
        return -1;
    }

    // replica a matriz (imagem), com nome negative
    cv::Mat negative(img.rows, img.cols, img.type());
    // percorre a matriz (pixeis)
    for (int i = 0; i < img.rows; ++i) {
        for (int j = 0; j < img.cols; ++j) {
            // cv::Vec3b -> vector de 3 bytes (BGR)
            cv::Vec3b color = img.at<cv::Vec3b>(i, j);
            cv::Vec3b negColor;
            // 255 - cor_original = cor negativa
            // faz isso para os 3 canais (B, G, R)
            negColor[0] = 255 - color[0];
            negColor[1] = 255 - color[1];
            negColor[2] = 255 - color[2];
            negative.at<cv::Vec3b>(i, j) = negColor;
        }
    }

    // Salvar a imagem
    if (!cv::imwrite(outputFile, negative)) {
        std::cerr << "Erro ao salvar a imagem: " << outputFile << std::endl;
        return -1;
    }

    // Se tiver view, abre a imagem para ver (dá jeito)
    if (viewImage) {
        cv::imshow("Imagem Negativa", negative);
        cv::waitKey(0);
    }

    return 0;
}
