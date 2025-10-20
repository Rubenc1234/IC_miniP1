# Caminhos
SRC_DIR := src
BIN_DIR := bin
OUT_DIR := output
TEST_FILE := test/sample.wav

EXEC_EFFECTS := $(BIN_DIR)/wav_effects
EXEC_QUANT := $(BIN_DIR)/wav_quant
EXEC_CMP := $(BIN_DIR)/wav_cmp
EXEC_HIST := $(BIN_DIR)/wav_hist

SRC_EFFECTS := $(SRC_DIR)/wav_effects.cpp
SRC_QUANT := $(SRC_DIR)/wav_quant.cpp
SRC_CMP := $(SRC_DIR)/wav_cmp.cpp
SRC_HIST := $(SRC_DIR)/wav_hist.cpp

# Efeitos
EFFECTS := echo multiecho am delay

# Arquivos de output gerados
OUT_FILES := $(foreach e,$(EFFECTS),$(OUT_DIR)/effect_$(e).wav)

# Compilador
CXX := g++
CXXFLAGS := -std=c++17 -Wall
LIBS := -lsndfile -lm

.PHONY: all clean effects quant cmp hist

# Default
all: $(EXEC_EFFECTS) $(EXEC_QUANT) $(EXEC_CMP) $(EXEC_HIST) effects quant cmp hist

# Compila os executáveis
$(EXEC_EFFECTS): $(SRC_EFFECTS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)
	@echo "Compiled $(EXEC_EFFECTS)"

$(EXEC_QUANT): $(SRC_QUANT)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)
	@echo "Compiled $(EXEC_QUANT)"

$(EXEC_CMP): $(SRC_CMP)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)
	@echo "Compiled $(EXEC_CMP)"

$(EXEC_HIST): $(SRC_HIST)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)
	@echo "Compiled $(EXEC_HIST)"

# Gera todos os efeitos
effects: $(EXEC_EFFECTS)
	@mkdir -p $(OUT_DIR)
	@for e in $(EFFECTS); do \
		outfile=$(OUT_DIR)/effect_$$e.wav; \
		echo "Applying $$e -> $$outfile"; \
		$(EXEC_EFFECTS) $(TEST_FILE) $$outfile $$e; \
	done

# Quantização (output na src)
quant: $(EXEC_QUANT)
	@echo "Running quantization..."
	$(EXEC_QUANT) $(TEST_FILE) $(SRC_DIR)/quant_out.wav 8

# Comparação
cmp: $(EXEC_CMP)
	@echo "Running comparison..."
	$(EXEC_CMP) $(SRC_DIR)/quant_out.wav $(TEST_FILE)

# Histograma
hist: $(EXEC_HIST)
	@echo "Running histogram..."
	$(EXEC_HIST) $(TEST_FILE) 0

# Limpeza
clean:
	rm -f $(EXEC_EFFECTS) $(EXEC_QUANT) $(EXEC_CMP) $(EXEC_HIST)
	rm -f $(OUT_FILES)
	rm -f $(SRC_DIR)/quant_out.wav
	rm -f output_hist*.txt
	@echo "Cleaned outputs and binaries"