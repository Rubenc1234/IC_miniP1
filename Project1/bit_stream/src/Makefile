# ============================================
# Makefile para WAV Quant/DCT Encoder & Decoder
# ============================================
# Exemplo de uso:
#   make enc sample.wav 8
#   make dec Output_Enc_8.enc 8 1 44100
#   make dct_enc sample.wav 8 0.6
#   make dct_dec Output_DCT_Enc_8.enc 8 1 44100
# ============================================

CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra

SRCS_COMMON := bit_stream.cpp byte_stream.cpp
ENC_SRC := wav_quant_enc.cpp
DEC_SRC := wav_quant_dec.cpp
ENC_SRC_DCT := DCT_enc_Wav.cpp
DEC_SRC_DCT := DCT_dec_Wav.cpp

ENC_BIN := wav_quant_enc
DEC_BIN := wav_quant_dec
ENC_BIN_DCT := DCT_Enc
DEC_BIN_DCT := DCT_Dec

# Caminhos fixos
SAMPLE_PATH := /home/$(USER)/Desktop/IC_miniP1/sndfile-example/test/
OUT_DIR      := /home/$(USER)/Desktop/IC_miniP1/bit_stream/Wav_EncDec_Tests/
OUT_DIR_DCT  := /home/$(USER)/Desktop/IC_miniP1/bit_stream/Wav_DCT_Tests/

.PHONY: all enc dec dct_enc dct_dec clean help

%:
	@:


# --------------------------------------------
# Alvo padrão
# --------------------------------------------
all: $(ENC_BIN) $(DEC_BIN) $(ENC_BIN_DCT) $(DEC_BIN_DCT)
	@echo "✅ Build completo!"

# --------------------------------------------
# Compilação dos binários principais
# --------------------------------------------
$(ENC_BIN): $(ENC_SRC) $(SRCS_COMMON)
	$(CXX) $(CXXFLAGS) $(ENC_SRC) $(SRCS_COMMON) -o $@
	@echo "Built $@"

$(DEC_BIN): $(DEC_SRC) $(SRCS_COMMON)
	$(CXX) $(CXXFLAGS) $(DEC_SRC) $(SRCS_COMMON) -o $@
	@echo "Built $@"

# --------------------------------------------
# Compilação dos binários DCT
# --------------------------------------------
$(ENC_BIN_DCT): $(ENC_SRC_DCT) $(SRCS_COMMON)
	$(CXX) $(CXXFLAGS) $(ENC_SRC_DCT) $(SRCS_COMMON) -lfftw3 -lsndfile -lm -o $@
	@echo "Built $@"

$(DEC_BIN_DCT): $(DEC_SRC_DCT) $(SRCS_COMMON)
	$(CXX) $(CXXFLAGS) $(DEC_SRC_DCT) $(SRCS_COMMON) -lfftw3 -lm -o $@
	@echo "Built $@"

# --------------------------------------------
# Execução: WAV Quant Encoder
# --------------------------------------------
# Uso: make enc sample.wav 8
enc: $(ENC_BIN)
	@if [ "$(word 2,$(MAKECMDGOALS))" = "" ] || [ "$(word 3,$(MAKECMDGOALS))" = "" ]; then \
		echo "Usage: make enc <input_wav_name> <n_bits>"; \
		echo "Example: make enc sample.wav 8"; \
		exit 1; \
	fi; \
	infile="$(SAMPLE_PATH)$(word 2,$(MAKECMDGOALS))"; \
	nbits=$(word 3,$(MAKECMDGOALS)); \
	outfile="$(OUT_DIR)Output_Enc_$${nbits}.enc"; \
	mkdir -p "$(OUT_DIR)"; \
	echo "🎧 Running encoder: ./$(ENC_BIN) $$infile $$outfile $$nbits"; \
	./$(ENC_BIN) "$$infile" "$$outfile" "$$nbits"

# --------------------------------------------
# Execução: WAV Quant Decoder
# --------------------------------------------
# Uso: make dec Output_Enc_8.enc 8 1 44100
dec: $(DEC_BIN)
	@if [ "$(word 2,$(MAKECMDGOALS))" = "" ] || [ "$(word 5,$(MAKECMDGOALS))" = "" ]; then \
		echo "Usage: make dec <enc_filename> <n_bits> <channels> <rate>"; \
		echo "Example: make dec Output_Enc_8.enc 8 1 44100"; \
		exit 1; \
	fi; \
	encfile="$(OUT_DIR)$(word 2,$(MAKECMDGOALS))"; \
	nbits=$(word 3,$(MAKECMDGOALS)); \
	ch=$(word 4,$(MAKECMDGOALS)); \
	rate=$(word 5,$(MAKECMDGOALS)); \
	outfile="$(OUT_DIR)Output_Dec_$${nbits}.wav"; \
	mkdir -p "$(OUT_DIR)"; \
	echo "🔊 Running decoder: ./$(DEC_BIN) $$encfile $$outfile $$nbits $$ch $$rate"; \
	./$(DEC_BIN) "$$encfile" "$$outfile" $$nbits $$ch $$rate

# --------------------------------------------
# Execução: DCT Encoder
# --------------------------------------------
# Uso: make dct_enc sample.wav 8 0.6
dct_enc: $(ENC_BIN_DCT)
	@if [ "$(word 2,$(MAKECMDGOALS))" = "" ] || [ "$(word 4,$(MAKECMDGOALS))" = "" ]; then \
		echo "Usage: make dct_enc <input_wav> <n_bits> <dctFrac>"; \
		echo "Example: make dct_enc sample.wav 8 0.6"; \
		exit 1; \
	fi; \
	infile="$(SAMPLE_PATH)$(word 2,$(MAKECMDGOALS))"; \
	nbits=$(word 3,$(MAKECMDGOALS)); \
	dctfrac=$(word 4,$(MAKECMDGOALS)); \
	outfile="$(OUT_DIR_DCT)Output_DCT_Enc_$${nbits}.enc"; \
	mkdir -p "$(OUT_DIR_DCT)"; \
	echo "🎶 Running DCT encoder: ./$(ENC_BIN_DCT) $$infile $$outfile $$nbits $$dctfrac"; \
	./$(ENC_BIN_DCT) "$$infile" "$$outfile" "$$nbits" "$$dctfrac"

# --------------------------------------------
# Execução: DCT Decoder
# --------------------------------------------
# Uso: make dct_dec Output_DCT_Enc_8.enc 8 1 44100
dct_dec: $(DEC_BIN_DCT)
	@if [ "$(word 2,$(MAKECMDGOALS))" = "" ] || [ "$(word 5,$(MAKECMDGOALS))" = "" ]; then \
		echo "Usage: make dct_dec <enc_filename> <n_bits> <channels> <rate>"; \
		echo "Example: make dct_dec Output_DCT_Enc_8.enc 8 1 44100"; \
		exit 1; \
	fi; \
	encfile="$(OUT_DIR_DCT)$(word 2,$(MAKECMDGOALS))"; \
	nbits=$(word 3,$(MAKECMDGOALS)); \
	ch=$(word 4,$(MAKECMDGOALS)); \
	rate=$(word 5,$(MAKECMDGOALS)); \
	outfile="$(OUT_DIR_DCT)Output_DCT_Dec_$${nbits}.wav"; \
	mkdir -p "$(OUT_DIR_DCT)"; \
	echo "🎵 Running DCT decoder: ./$(DEC_BIN_DCT) $$encfile $$outfile $$nbits $$ch $$rate"; \
	./$(DEC_BIN_DCT) "$$encfile" "$$outfile" $$nbits $$ch $$rate

# --------------------------------------------
# Limpeza
# --------------------------------------------
clean:
	@echo "🧹 Cleaning build and outputs..."; \
	rm -f $(ENC_BIN) $(DEC_BIN) $(ENC_BIN_DCT) $(DEC_BIN_DCT); \
	rm -f $(OUT_DIR)Output_Enc_*.enc $(OUT_DIR)Output_Dec_*.wav; \
	rm -f $(OUT_DIR_DCT)Output_DCT_Enc_*.enc $(OUT_DIR_DCT)Output_DCT_Dec_*.wav; \
	echo "✅ Cleaned."

# --------------------------------------------
# Ajuda
# --------------------------------------------
help:
	@echo "📘 Exemplos:"; \
	 echo "  make enc sample.wav 8"; \
	 echo "  make dec Output_Enc_8.enc 8 1 44100"; \
	 echo "  make dct_enc sample.wav 8 0.6"; \
	 echo "  make dct_dec Output_DCT_Enc_8.enc 8 1 44100"
