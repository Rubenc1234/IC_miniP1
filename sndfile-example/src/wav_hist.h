//------------------------------------------------------------------------------
//
// Copyright 2025 University of Aveiro, Portugal, All Rights Reserved.
//
// These programs are supplied free of charge for research purposes only,
// and may not be sold or incorporated into any commercial product. There is
// ABSOLUTELY NO WARRANTY of any sort, nor any undertaking that they are
// fit for ANY PURPOSE WHATSOEVER. Use them at your own risk. If you do
// happen to find a bug, or have modifications to suggest, please report
// the same to Armando J. Pinho, ap@ua.pt. The copyright notice above
// and this statement of conditions must remain an integral part of each
// and every copy made of these files.
//
// Armando J. Pinho (ap@ua.pt)
// IEETA / DETI / University of Aveiro
//
#ifndef WAVHIST_H
#define WAVHIST_H

#include <iostream>
#include <vector>
#include <map>
#include <sndfile.hh>

#include <fstream>
#include <cmath>

class WAVHist {
  private:
	std::vector<std::map<short, size_t>> counts;
	std::vector<std::map<short, size_t>> counts_mid_side;
	// agrupar bins de histograma
	short bins = 5; // 2^5 = 32
	short bin_coarser = std::pow(2, bins);
	std::string output_file = "output_hist";

  public:
	WAVHist(const SndfileHandle& sfh) {
		counts.resize(sfh.channels());
		counts_mid_side.resize(1);
		 if(sfh.channels() == 2) {
            counts_mid_side.resize(2); // cria os mapas para MID e SIDE
        }
	}

	void update(const std::vector<short>& samples) {
		size_t n { };
		for(auto s : samples)
			counts[n++ % counts.size()][s]++;
		// mono -> mid (1 canal)
		if (counts.size() == 1)
		{
			for(size_t i = 0 ; i < samples.size() ; ++i) {
				counts_mid_side[0][samples[i]/bin_coarser]++;
			}
		}
		
		// stereo -> mid + side (2 canais)
		else if(counts.size() == 2) {
			// samples: L R L R L R ...
			for(size_t i = 0 ; i < samples.size() ; i += 2) {
				// calcula mid e side, com amostras L e R de samples
				// incrementa os contadores
				short mid = MID(samples[i], samples[i+1]);
				short side = SIDE(samples[i], samples[i+1]);
				counts_mid_side[0][mid/bin_coarser]++;
				counts_mid_side[1][side/bin_coarser]++;
			}
		}
	}

	void dump(const size_t channel) const {
		std::ofstream out(output_file + "_channel_" + std::to_string(channel) + ".txt");
		for(auto [value, counter] : counts[channel])
			out << value << '\t' << counter << '\n';
        out.close();
		
		// se for mono ou stereo, mostra mid
		std::ofstream out_mid(output_file + "_mid.txt");
		for(auto [value, counter] : counts_mid_side[0])
			out_mid << value << '\t' << counter << '\n';
		out_mid.close();

		// se for stereo, mostra mid e side
		if(counts_mid_side.size() >= 2) {
			std::ofstream out_side(output_file + "_side.txt");
			for(auto [value, counter] : counts_mid_side[1])
				out_side << value << '\t' << counter << '\n';
        	out_side.close();
    	}
	}

	short MID(short L, short R) {
		return (L + R) / 2;
	}
	short SIDE(short L, short R) {
		return (L - R) / 2;
	}
};

#endif

