#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>

using namespace std;

struct Image {
    int width = 0;
    int height = 0;
    int maxval = 255;
    int channels = 1; // 1 = grayscale, 3 = RGB
    vector<vector<int>> data;
    vector<vector<vector<int>>> data3D;
};

// PPM / PGM I/O
bool readPPMtoGray(const string& filename, Image& img);
bool writeGrayAsPPM(const string& filename, const Image& img);

// predictor + mappings
int predict(int a, int b, int c);
int residualToUnsigned(int r);
int unsignedToResidual(int n);

#endif // UTILS_H
