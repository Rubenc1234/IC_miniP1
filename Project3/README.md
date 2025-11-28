# IC - Trabalho Laboratorial nº 3 (2025/26)

Este repositório contém o desenvolvimento do terceiro trabalho laboratorial da unidade curricular de Informação e Codificação (IC) da Universidade de Aveiro. O projeto foca-se na compressão de dados, utilizando técnicas como a codificação Golomb e estratégias de pré-processamento para otimizar a compressão de imagens e áudio.

---

## Estrutura do Repositório

O projeto está organizado da seguinte forma:

```text
.
├── .gitignore          # Ficheiros ignorados pelo Git
├── Makefile            # Automatiza a compilação do código C++
├── README.md           # Este ficheiro
├── bin/                # Diretoria para os executáveis compilados
├── build/              # Diretoria para os binários intermediários
├── data/               # Dados de entrada, como o ficheiro model.safetensors
├── doc/                # Documentação do projeto
│   ├── blueprint.md    # Planeamento e estratégia do projeto
│   ├── relatorio/      # Ficheiros relativos ao relatório LaTeX
│   │   ├── build/      # Ficheiros auxiliares e PDF gerado pelo LaTeX
│   │   ├── imagens/    # Imagens usadas no relatório
│   │   └── relatorio.tex # Código fonte do relatório
├── output/             # Diretoria para os resultados gerados (imagens, áudio, etc.)
├── src/                # Código fonte C++ (.cpp, .h)
│   ├── 01_extract_header.cpp
│   ├── 02_calc_global.cpp
│   └── 03_analyze_split.cpp
└── plot_histogram.gnu  # Script para gerar gráficos com Gnuplot
```

---

## Dependências

Para compilar e executar este projeto, necessita de:

1. **Compilador C++:** Um compilador moderno que suporte C++17 (e.g., `g++` ou `clang++`).
2. **Make:** A ferramenta `make` para utilizar o `Makefile`.
3. **OpenCV (Versão 4.x):** Biblioteca para processamento de imagem.
4. **pkg-config:** Ferramenta auxiliar para encontrar as flags de compilação do OpenCV.
5. **LaTeX:** Uma distribuição LaTeX (como TeX Live) para compilar o relatório (`relatorio.tex`).

**Instalação em Debian/Ubuntu:**

```bash
sudo apt update
sudo apt install build-essential make pkg-config libopencv-dev texlive-full
```

---

## Compilação do Código C++

A compilação dos programas C++ é gerida pelo `Makefile` fornecido.

1. Navegue até à raiz do repositório no terminal.
2. Execute o comando `make`:

```bash
make
```

Isto compilará todos os ficheiros `.cpp` encontrados na pasta `src/` e colocará os executáveis correspondentes na pasta `bin/`.

Para limpar os ficheiros compilados, execute:

```bash
make clean
```

---

## Compilação do Relatório LaTeX

O relatório (`relatorio.tex`) encontra-se na pasta `doc/relatorio/`. Para compilar o PDF:

1. Navegue até à pasta do relatório:

```bash
cd doc/relatorio
```

2. Execute o `pdflatex` (pode precisar de executar duas vezes para que as referências e o índice fiquem corretos):

```bash
pdflatex -output-directory=build -aux-directory=build -jobname=relatorio relatorio.tex && mv build/relatorio.pdf relatorio.pdf
```

O ficheiro PDF final (`relatorio.pdf`) estará disponível em `doc/relatorio/`.

---

## Execução dos Programas

Os executáveis encontram-se na pasta `bin/`. Execute-os a partir da raiz do projeto.

### Exemplos:

**Extrair Cabeçalho:**

```bash
./bin/01_extract_header data/model.safetensors output/header.json
```

**Calcular Estatísticas Globais:**

```bash
./bin/02_calc_global data/model.safetensors output/global_stats.json
```

**Analisar Divisão de Bytes:**

```bash
./bin/03_analyze_split data/model.safetensors output/byte_analysis.json
```

**Gerar Gráficos com Gnuplot:**

```bash
gnuplot plot_histogram.gnu
```

---

## Autores

* **Pedro Miguel Miranda de Melo** (114208)
* **Rúben Cardeal Costa** (114190)
* **Hugo Marques Dias** (114142)