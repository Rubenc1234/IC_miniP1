#include <iostream>
#include <vector>
#include <sndfile.hh>
#include "wav_hist.h"

using namespace std;

constexpr size_t FRAMES_BUFFER_SIZE = 65536; // Buffer for reading frames

int main(int argc, char *argv[]) {

	if(argc < 4) {
		cerr << "Usage: " << argv[0] << " <input file> <output file> <effect>\n";
		cerr << "Effects: echo, multiecho, am, delay\n";
		return 1;
	}

	string inputFile = argv[1];
    string outputFile = argv[2];
    string effect = argv[3];

    // Abrir input file
    SndfileHandle sfhIn { inputFile };
    if(sfhIn.error()) { 
		cerr << "Invalid input file\n"; return 1; 
	}

    if((sfhIn.format() & SF_FORMAT_TYPEMASK) != SF_FORMAT_WAV ||
       (sfhIn.format() & SF_FORMAT_SUBMASK) != SF_FORMAT_PCM_16) {
        cerr << "Only PCM_16 WAV files are supported.\n";
        return 1;
    }

    // Criar output file
    SndfileHandle sfhOut { outputFile, SFM_WRITE, sfhIn.format(),
                            sfhIn.channels(), sfhIn.samplerate() };
    if(sfhOut.error()) { 
		cerr << "Invalid output file\n"; return 1; 
	}

	vector<short> samples(sfhIn.frames() * sfhIn.channels());
	sfhIn.readf(samples.data(), sfhIn.frames());
	vector<float> out(samples.size(), 0.0f);


	size_t ch = sfhIn.channels();
	size_t sr = sfhIn.samplerate();


	// echo -> new_x[n] = x[n] + alfa * x[n - delay*channels]
	// *ch -> porque o delay é em frames e não em samples
	if (effect == "echo") {
		float alfa = 0.7f;
		int delay = sr/4;
		for (size_t i = 0; i < samples.size(); i++)
		{
			out[i] = samples[i];
			if (i >= delay*ch)
			{
				out[i] += alfa * samples[i - delay*ch];
			}
			
		}
	// multiecho -> new_x[n] = x[n] + alfa^1 * x[n - delay*1*channels] + alfa^2 * x[n - delay*2*channels] + ...
	} else if (effect == "multiecho") {
		float alfa = 0.7f;
		int delay = sr/4;	// 0.25s
		int n_echos = 5;
		for (size_t i = 0; i < samples.size(); i++)
		{
			out[i] = samples[i];
			
			for (int n = 1; n <= n_echos; n++)
			{
				if (i >= n*delay*ch)
				{
					out[i] += pow(alfa, n) * samples[i - n*delay*ch];
				}
				
			}
		}
	// am -> new_x[n] = x[n] * (1 + depth * sin(2*pi*fm*t))
	// t -> n/sr 
	} else if (effect == "am") {
		float depth = 0.5f;
		float fm = 5.0f;   // 5 Hz
		for (size_t i = 0; i < samples.size(); i++)
		{
			out[i] = samples[i] * (1 + depth * sin(2 * M_PI * fm * ((float)i / (float)sr)));
		}
	
	// delay -< new_x[n] = delay_0 + depth * sin(2*pi*fm*t)
	} else if (effect == "delay")
	{
		float delayBase = 0.01f;
		float depth = 0.1f;
		float fm = 0.5f;
		float intensity = 0.5f;
		for (size_t i = 0; i < samples.size(); i++)
		{
			int delaySamples = (int)((delayBase + depth * sin(2*M_PI*fm*(float)i/sr)) * sr);
			if (i >= delaySamples*ch)
				out[i] = samples[i] + intensity * samples[i - delaySamples*ch];
			else
				out[i] = samples[i];
		}
	} else {
		cerr << "Unknown effect: " << effect << "\n";
		return 1;
	}
	
	// normalizar os valores
	float maxVal = 0.0f;
	// procura o maior valor abs
    for (float v : out) {
		maxVal = max(maxVal, fabs(v));
	}
    if (maxVal > 32767.0f) {
        float scale = 32767.0f / maxVal;

        for (auto &v : out) {
			v *= scale;
		}
    }

	// Converter de volta para short
    vector<short> outSamples(out.size());
    for(size_t i = 0; i < out.size(); i++)
        outSamples[i] = static_cast<short>(out[i]);

    // Escrever output file
    sfhOut.writef(outSamples.data(), sfhIn.frames());
    cout << "Effect applied: " << effect << " -> saved in " << outputFile << endl;

    return 0;
}