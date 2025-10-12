#include <iostream>
#include <vector>
#include <sndfile.hh>
#include <cmath>

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading/writing frames

int main(int argc, char *argv[]) {

    bool verbose { false };

	if(argc < 3) {
		cerr << "Usage: wav_cmp [ -v (verbose) ]\n";
		cerr << "               wavFileModified wavFileIn\n";
		return 1;
	}

	for(int n = 1 ; n < argc ; n++)
		if(string(argv[n]) == "-v") {
			verbose = true;
			break;
		}
    
    SndfileHandle sfhMod { argv[argc-2] };
	if(sfhMod.error()) {
		cerr << "Error: invalid Modinput file\n";
		return 1;
    }

	if((sfhMod.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV) {
		cerr << "Error: Modfile is not in WAV format\n";
		return 1;
	}

	if((sfhMod.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
		cerr << "Error: file is not in PCM_16 format\n";
		return 1;
	}

	SndfileHandle sfhIn { argv[argc-1] };
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

	if(verbose) {
		cout << "Input file has:\n";
		cout << '\t' << sfhIn.frames() << " frames\n";
		cout << '\t' << sfhIn.samplerate() << " samples per second\n";
		cout << '\t' << sfhIn.channels() << " channels\n";
	}

    // para cada canal e para cada média de canais:
    size_t nFrames;
	vector<short> samplesIn(FRAMES_BUFFER_SIZE * sfhIn.channels());
    vector<short> samplesMod(FRAMES_BUFFER_SIZE * sfhMod.channels());
    //short counts = sfhIn.channels();

    size_t channels = sfhIn.channels();
    vector<long long> sumError2(channels, 0);    // soma dos erros²
    vector<long long> signalEnergy(channels, 0); // soma dos x²
    vector<double> maxError(channels, 0.0);           // erro absoluto máximo
    long long totalSamples = 0;



    while((nFrames = sfhIn.readf(samplesIn.data(), FRAMES_BUFFER_SIZE))) {
        // numero de samples lidos
        size_t nFramesMod = sfhMod.readf(samplesMod.data(), FRAMES_BUFFER_SIZE);
        if (nFrames != nFramesMod) {
            cerr << "Error: files have different length\n";
            return 1;
        }

        size_t nSamples = nFrames * sfhIn.channels();

            for (size_t i = 0; i < nSamples; ++i) {
                short x = samplesIn[i];   // original
                short y = samplesMod[i];  // modificado

                double error = static_cast<double>(x) - static_cast<double>(y);
                // canal a que o sample pertence
                size_t ch = i % channels;
                // sum em cada canal
                sumError2[ch] += 1LL * error * error;           // MSE
                signalEnergy[ch] += 1LL * x * x;                // SNR
                maxError[ch] = max(maxError[ch], fabs(error));   // E_inf

                totalSamples++;
        }

    }

    // The average mean squared error between a certain audio file and its original version
    // MSE = 1/N * sum( (x_i - y_i)^2 ) -> sum(error^2) / N

    // The maximum per sample absolute error
    // E_inf = max |x_i - y_i| -> max(abs(error))

    // The signal-to-noise ratio (SNR) of a certain audio file in relation to its original version
    // SNR = 10 * log10 ( sum(x_i^2) / sum( (x_i - y_i)^2 ) ) -> 10 * log10 ( signalEnergy / sumError2 ) && sumError2 = sum(error)^2


    for (size_t ch = 0; ch < channels; ++ch) {
        double mse = static_cast<double>(sumError2[ch]) / (sfhIn.frames());
        double linf = maxError[ch];
        double snr = 10.0 * log10(
            static_cast<double>(signalEnergy[ch]) / sumError2[ch]
        );

        cout << "Channel " << ch << ":\n";
        cout << "\tMSE  = " << mse << "\n";
        cout << "\tLinf = " << linf << "\n";
        cout << "\tSNR  = " << snr << " dB\n";
    }

    long long sumErrAll = 0, sigAll = 0;
    double maxAll = 0;

    for (size_t ch = 0; ch < channels; ++ch) {
        sumErrAll += sumError2[ch];
        sigAll += signalEnergy[ch];
        maxAll = max(maxAll, maxError[ch]);
    }

    double mseAll = static_cast<double>(sumErrAll) / (sfhIn.frames() * channels);
    double snrAll = 10.0 * log10(static_cast<double>(sigAll) / sumErrAll);

    cout << "=== Average over channels ===\n";
    cout << "\tMSE  = " << mseAll << "\n";
    cout << "\tLinf = " << maxAll << "\n";
    cout << "\tSNR  = " << snrAll << " dB\n";
	return 0;
}
