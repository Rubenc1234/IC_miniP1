# Makefile corrigido: usa SAMPLE_PATH e OUT_DIR e aceita comandos como:
#   make enc sample.wav 4
#   make dec Output_Enc_4.enc 4 2 44100

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

SRCS_COMMON := bit_stream.cpp byte_stream.cpp
ENC_SRC := wav_quant_enc.cpp
DEC_SRC := wav_quant_dec.cpp

ENC_BIN := wav_quant_enc
DEC_BIN := wav_quant_dec

# Caminhos fixos
SAMPLE_PATH := /home/$(USER)/Desktop/IC_miniP1/sndfile-example/test/
OUT_DIR     := /home/$(USER)/Desktop/IC_miniP1/bit_stream/Wav_EncDec_Tests/

.PHONY: all enc dec clean help

# catch-all rule: evita que make tente construir argumentos posicionais como targets
%:
	@:

all: $(ENC_BIN) $(DEC_BIN)
	@echo "Build complete."

# compile encoder
$(ENC_BIN): $(ENC_SRC) $(SRCS_COMMON)
	$(CXX) $(CXXFLAGS) $(ENC_SRC) $(SRCS_COMMON) -o $@
	@echo "Built $@"

# compile decoder
$(DEC_BIN): $(DEC_SRC) $(SRCS_COMMON)
	$(CXX) $(CXXFLAGS) $(DEC_SRC) $(SRCS_COMMON) -o $@
	@echo "Built $@"

# --- ENCODER ---
# Usage: make enc sample.wav 4
enc: $(ENC_BIN)
	@if [ "$(word 2,$(MAKECMDGOALS))" = "" ] || [ "$(word 3,$(MAKECMDGOALS))" = "" ]; then \
		echo "Usage: make enc <input_wav_name> <n_bits>"; \
		echo "Example: make enc sample.wav 8"; \
		exit 1; \
	fi; \
	infile=$(SAMPLE_PATH)$(word 2,$(MAKECMDGOALS)); \
	nbits=$(word 3,$(MAKECMDGOALS)); \
	outfile=$(OUT_DIR)Output_Enc_$${nbits}.enc; \
	mkdir -p $(OUT_DIR); \
	echo "Running encoder: ./$(ENC_BIN) $$infile $$outfile $$nbits"; \
	./$(ENC_BIN) "$$infile" "$$outfile" "$$nbits"; \
	exit 0

# --- DECODER ---
# Usage: make dec Output_Enc_4.enc 4 2 44100
dec: $(DEC_BIN)
	@if [ "$(word 2,$(MAKECMDGOALS))" = "" ] || [ "$(word 5,$(MAKECMDGOALS))" = "" ]; then \
		echo "Usage: make dec <enc_filename> <n_bits> <channels> <rate>"; \
		echo "Example: make dec Output_Enc_4.enc 4 2 44100"; \
		exit 1; \
	fi; \
	encfile=$(OUT_DIR)$(word 2,$(MAKECMDGOALS)); \
	nbits=$(word 3,$(MAKECMDGOALS)); \
	ch=$(word 4,$(MAKECMDGOALS)); \
	rate=$(word 5,$(MAKECMDGOALS)); \
	outfile=$(OUT_DIR)Output_Dec_$${nbits}.wav; \
	mkdir -p $(OUT_DIR); \
	echo "Running decoder: ./$(DEC_BIN) $$encfile $$outfile $$nbits $$ch $$rate"; \
	./$(DEC_BIN) "$$encfile" "$$outfile" $$nbits $$ch $$rate; \
	exit 0

clean:
	@echo "Cleaning..."; \
	rm -f $(ENC_BIN) $(DEC_BIN) $(OUT_DIR)Output_Enc_*.enc $(OUT_DIR)Output_Dec_*.wav; \
	@echo "Cleaned."

help:
	@echo "Examples:"; \
	 echo "  make enc sample.wav 4"; \
	 echo "    -> will use $(SAMPLE_PATH)sample.wav and create $(OUT_DIR)Output_Enc_4.enc"; \
	 echo "  make dec Output_Enc_4.enc 4 2 44100"; \
	 echo "    -> will read $(OUT_DIR)Output_Enc_4.enc and create $(OUT_DIR)Output_Dec_4.wav"
