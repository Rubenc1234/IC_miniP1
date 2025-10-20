#include <iostream>
#include <vector>
#include <sndfile.hh>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading/writing frames

int main(int argc, char *argv[]) {

    bool verbose { false };
    short b = 16;

	if(argc < 4) {
		cerr << "Usage: wav_dct [ -v (verbose) ]\n";
		cerr << "               wavFileIn wavFileOut b\n";
		return 1;
	}

	for(int n = 1 ; n < argc ; n++)
		if(string(argv[n]) == "-v") {
			verbose = true;
			break;
		}

	SndfileHandle sfhIn { argv[argc-3] };
	if(sfhIn.error()) {
		cerr << "Error: invalid input file\n";
		return 1;
    }

	if((sfhIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
		cerr << "Error: file is not in WAV format\n";
		return 1;
	}

	if((sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
		cerr << "Error: file is not in PCM_16 format\n";
		return 1;
	}

	SndfileHandle sfhOut { argv[argc-2], SFM_WRITE, sfhIn.format(),
	  sfhIn.channels(), sfhIn.samplerate() };
	if(sfhOut.error()) {
		cerr << "Error: invalid output file\n";
		return 1;
    }

    short b_arg = atoi(argv[argc-1]);
    if(b_arg < 1 || b_arg > 16) {
        cerr << "Error: b must be in the range [1, 16]\n";
        return 1;
    }

	if(verbose) {
		cout << "Input file has:\n";
		cout << '\t' << sfhIn.frames() << " frames\n";
		cout << '\t' << sfhIn.samplerate() << " samples per second\n";
		cout << '\t' << sfhIn.channels() << " channels\n";
	}

    size_t nFrames;
	vector<short> samples(FRAMES_BUFFER_SIZE * sfhIn.channels());

    int shift = 16 - b_arg; // quantos bits vamos descartar
    int step  = 1 << shift; // Δ = 2^(16-b) -> intervalo de amplitude

    // guarda em samples os frames lidos (com o tamanho determinado)
    while((nFrames = sfhIn.readf(samples.data(), FRAMES_BUFFER_SIZE))) {
        // numero de samples lidos
        size_t nSamples = nFrames * sfhIn.channels();

        // arredondar para o nivel step mais próximo -> q = round(x/Δ)*Δ
        for(size_t i = 0; i < nSamples; ++i) {
            short x = samples[i];
            int q = (x >= 0) ? ((x + step/2) / step) * step
                            : ((x - step/2) / step) * step;
			if (q > 32767) q = 32767;
			if (q < -32768) q = -32768;
            // converter de int para short
            samples[i] = static_cast<short>(q);
        }

        sfhOut.writef(samples.data(), nFrames);
    }

    if(verbose) {
        cout << "Quantization done! Output saved as " << argv[argc-2] << "\n";
    }

	return 0;
}