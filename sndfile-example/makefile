# Caminhos
SRC_DIR := src
BIN_DIR := bin
OUT_DIR := output
TEST_FILE := test/sample.wav
EXEC := $(BIN_DIR)/wav_effects
SRC := $(SRC_DIR)/wav_effects.cpp

# Efeitos
EFFECTS := echo multiecho am delay

# Arquivos de output gerados
OUT_FILES := $(foreach e,$(EFFECTS),$(OUT_DIR)/effect_$(e).wav)

# Compilador
CXX := g++
CXXFLAGS := -std=c++17 -Wall
LIBS := -lsndfile -lm

.PHONY: all clean effects

# Default
all: $(EXEC) effects

# Compila o executável
$(EXEC): $(SRC)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)
	@echo "Compiled $(EXEC)"

# Gera todos os efeitos
effects: $(EXEC)
	@mkdir -p $(OUT_DIR)
	@for e in $(EFFECTS); do \
		outfile=$(OUT_DIR)/effect_$$e.wav; \
		echo "Applying $$e -> $$outfile"; \
		$(EXEC) $(TEST_FILE) $$outfile $$e; \
	done

# Limpeza
clean:
	rm -f $(EXEC)
	rm -f $(OUT_FILES)
	@echo "Cleaned outputs and binary"
