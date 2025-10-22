#include <opencv2/opencv.hpp>
#include <iostream>

/*
g++ mirrored.cpp -o mirrored `pkg-config --cflags --libs opencv4`
./mirrored imagens\ PPM/airplane.ppm mirrored.jpg h view
*/

int main(int argc, char** argv) {
    if (argc <4) {    
        std::cerr << "Uso: " << argv[0] << " <arquivo_entrada> <arquivo_saida> <[h or v]> [view]" << std::endl;
        return 1;
    }

    std::string inputFile = argv[1];
    std::string outputFile = argv[2];
    char choice = argv[3][0];
    bool viewImage = (argc >= 5 && std::string(argv[4]) == "view");

    // cv::Mat -> imagem como matriz
    cv::Mat img = cv::imread(inputFile);
    if (img.empty()) {
        std::cerr << "Não foi possível abrir a imagem: " << inputFile << std::endl;
        return -1;
    }

    cv::Mat mirrored(img.rows, img.cols, img.type());
    // percorre a matriz (pixeis)
    for (int i = 0; i < img.rows; ++i) {
        for (int j = 0; j < img.cols; ++j) {
            if (choice == 'h')
            {
                // pixel_espelhado(i,j)=pixel_original(i,largura−j−1)
                mirrored.at<cv::Vec3b>(i, j) = img.at<cv::Vec3b>(i, img.cols - j - 1);
            } 
            else if (choice == 'v')
            {
                // pixel_espelhado(i,j)=pixel_original(altura−i−1,j)
                mirrored.at<cv::Vec3b>(i, j) = img.at<cv::Vec3b>(img.rows - i -1, j);
            } 
            else {
                std::cerr << "Escolha inválida: " << choice << ". Use 'h' ou 'v'." << std::endl;
                return -1;
            }
            
            
            
        }
    }

    // Salvar a imagem
    if (!cv::imwrite(outputFile, mirrored)) {
        std::cerr << "Erro ao salvar a imagem: " << outputFile << std::endl;
        return -1;
    }

    // Se tiver view, abre a imagem para ver (dá jeito)
    if (viewImage) {
        cv::imshow("Imagem Espelhada", mirrored);
        cv::waitKey(0);
    }

    std::cout << "Imagem espelhada salva como: " << outputFile << std::endl;

    return 0;
}
