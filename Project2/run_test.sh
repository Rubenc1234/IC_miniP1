#!/bin/bash

# Diretório base
PROJECT_DIR="/home/pedromelo/Documents/UA/4ano/1semestre/IC/P/IC-Trabalho2/grupo/IC_miniP1/Project2"

# Mudar para o diretório do projeto
cd "$PROJECT_DIR" || { echo "Erro: Não foi possível mudar para o diretório do projeto."; exit 1; }

echo "=== 🚀 Início da Codificação de Imagens ==="
echo ""

# 1. Codificar airplane.ppm
echo "-> Codificando img/airplane.ppm"
time ./bin/image_encoder img/airplane.ppm out/airplane.gol
echo "-----------------------------------"

# 2. Codificar arial.ppm
echo "-> Codificando img/arial.ppm"
time ./bin/image_encoder img/arial.ppm out/arial.gol
echo "-----------------------------------"

# 3. Codificar bike3.ppm
echo "-> Codificando img/bike3.ppm"
time ./bin/image_encoder img/bike3.ppm out/bike3.gol
echo "-----------------------------------"

echo ""
echo "=== 🔄 Início da Descodificação de Imagens ==="
echo ""

# 4. Descodificar airplane.gol
echo "-> Descodificando out/airplane.gol"
time ./bin/image_decoder out/airplane.gol out/airplane_gray.ppm
echo "-----------------------------------"

# 5. Descodificar arial.gol
echo "-> Descodificando out/arial.gol"
time ./bin/image_decoder out/arial.gol out/arial_gray.ppm
echo "-----------------------------------"

# 6. Descodificar bike3.gol
echo "-> Descodificando out/bike3.gol"
time ./bin/image_decoder out/bike3.gol out/bike3_gray.ppm
echo "-----------------------------------"

echo ""
echo "=== 📊 Verificação dos Tamanhos dos Ficheiros ==="
echo ""

# 7. Listar tamanhos para airplane
echo "-> Tamanhos para airplane:"
ls -lh img/airplane.ppm out/airplane.gol out/airplane_gray.ppm
echo "-----------------------------------"

# 8. Listar tamanhos para arial
echo "-> Tamanhos para arial:"
ls -lh img/arial.ppm out/arial.gol out/arial_gray.ppm
echo "-----------------------------------"

# 9. Listar tamanhos para bike3
echo "-> Tamanhos para bike3:"
ls -lh img/bike3.ppm out/bike3.gol out/bike3_gray.ppm
echo "-----------------------------------"

echo "=== ✅ Script Concluído ==="