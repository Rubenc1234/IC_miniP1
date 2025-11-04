# Makefile para o Trabalho Laboratorial 2 de IC

# Compilador C++
CXX = g++

# Flags de Compilação (Otimização O2, Avisos Wall, C++17 Standard)
CXXFLAGS = -O2 -Wall -std=c++17

# Diretórios
SRCDIR = src
BINDIR = bin
IMGDIR = img
OUTDIR = out

# Encontrar flags do OpenCV usando pkg-config
OPENCV_CFLAGS = $(shell pkg-config --cflags opencv4)
OPENCV_LIBS = $(shell pkg-config --libs opencv4)

# Adicionar flags do OpenCV às flags gerais
CXXFLAGS += $(OPENCV_CFLAGS)
LIBS = $(OPENCV_LIBS)

# Lista de executáveis a serem criados na diretoria bin/
EXECUTABLES = $(BINDIR)/extract_channel \
              $(BINDIR)/image_negative \
              $(BINDIR)/image_mirror \
              $(BINDIR)/image_rotate \
			  $(BINDIR)/image_intensity \
			  $(BINDIR)/test_golomb
			  
# Alvo principal: compila todos os executáveis
all: $(EXECUTABLES)

# Regra para criar a diretoria bin/ se não existir
$(BINDIR):
	@mkdir -p $(BINDIR)

# Regra genérica para compilar um .cpp de src/ para um executável em bin/
$(BINDIR)/%: $(SRCDIR)/%.cpp | $(BINDIR)
	$(CXX) $(CXXFLAGS) $< -o $@ $(LIBS)

# Regra específica para compilar o test_golomb, pois ele depende de dois .cpp
$(BINDIR)/test_golomb: $(SRCDIR)/main_test_golomb.cpp $(SRCDIR)/Golomb.cpp $(SRCDIR)/Golomb.h | $(BINDIR)
	$(CXX) $(CXXFLAGS) $(SRCDIR)/main_test_golomb.cpp $(SRCDIR)/Golomb.cpp -o $@ $(LIBS)

# Alvo para limpar os executáveis compilados
clean:
	@echo "Limpando executáveis..."
	@rm -f $(EXECUTABLES)
	@echo "Limpeza concluída."

# Phony targets (alvos que não representam ficheiros)
.PHONY: all clean