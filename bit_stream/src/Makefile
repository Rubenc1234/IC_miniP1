all:
	@(if [ ! -d build ]; then mkdir build; fi; cd build; cmake ..; make)
	enc dec
	@echo "Build complete: $(ENC) $(DEC)"

clean:
	rm -rf build ../bin
	-rm -f $(ENC) $(DEC)
	@echo "Cleaned."


# Makefile - compilar e executar encoder/decoder de bitstream
CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

# Sources common
SRCS_COMMON := bit_stream.cpp byte_stream.cpp

# Encoder/Decoder sources
ENC_SRC := wav_quant_enc.cpp
DEC_SRC := wav_quant_dec.cpp

# Binaries
ENC_BIN := wav_quant_enc
DEC_BIN := wav_quant_dec

# Default file args (match the commands you gave)
enc_input  ?= ~/Desktop/IC_miniP1/sndfile-example/test/sample.wav
enc_output ?= ~/Desktop/IC_miniP1/bit_stream/Wav_EncDec_Tests/Output8bit.enc

dec_input  	?= ~/Desktop/IC_miniP1/bit_stream/Wav_EncDec_Tests/Output8bit.enc
dec_output 	?= ~/Desktop/IC_miniP1/bit_stream/Wav_EncDec_Tests/Sample8bit.wav
nbits 		?= 8   # In your example you passed 16 as n_bits for the decoder
channels    ?= 2
rate    	?= 44100

.PHONY: all enc dec run_enc run_dec run_both clean


# Build encoder
enc: $(SRCS_COMMON) $(ENC_SRC)
	$(CXX) $(CXXFLAGS) $(ENC_SRC) $(SRCS_COMMON) -o $(ENC_BIN)
	@echo "Built $(ENC_BIN)"

# Build decoder
dec: $(SRCS_COMMON) $(DEC_SRC)
	$(CXX) $(CXXFLAGS) $(DEC_SRC) $(SRCS_COMMON) -o $(DEC_BIN)
	@echo "Built $(DEC_BIN)"

# Run encoder (compiles first)
run_enc: enc
	@echo "Running encoder: ./$(ENC_BIN) $(enc_input) $(enc_output)"
	@./$(ENC_BIN) $(enc_input) $(enc_output)

# Run decoder (compiles first)
run_dec: dec
	@echo "Running decoder: ./$(DEC_BIN) $(dec_input) $(dec_output) $(nbits) $(channels) $(rate)"
	@./$(DEC_BIN) $(dec_input) $(dec_output) $(nbits) $(channels) $(rate)

# Run both in sequence (encoder then decoder) - useful if you want a single pipeline
# You can override any variable in the command line
run_both: enc dec
	@echo "Running encoder: ./$(ENC_BIN) $(enc_input) $(enc_output)"
	@./$(ENC_BIN) $(enc_input) $(enc_output)
	@echo "Running decoder: ./$(DEC_BIN) $(enc_output) $(dec_output) $(nbits) $(channels) $(rate)"
	@./$(DEC_BIN) $(enc_output) $(dec_output) $(nbits) $(channels) $(rate)