/**
 * @file decoder.cpp
 * @brief Descompressor de ficheiros SafeTensors comprimidos (.sc)
 * 
 * Estratégia de Descompressão:
 * 1. Lê o header JSON do SafeTensors e copia-o intacto
 * 2. Deteta o modo de compressão usado (Fast/Best)
 * 3. Para cada bloco:
 *    - Descomprime MSB usando Huffman ou Aritmética
 *    - Descomprime LSB (Raw ou RLE)
 *    - Intercala MSB/LSB para reconstruir valores 16-bit
 * 
 * @author IC-Trabalho2 Grupo
 * @date 2024
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <chrono>
#include <iomanip>

// ============================================================================
// CONFIGURAÇÕES GLOBAIS
// ============================================================================

namespace Config {
    constexpr size_t FREQ_TABLE_SIZE = 256 * 4;  // 1024 bytes para tabela de frequências
    constexpr int PROGRESS_INTERVAL = 100;        // Mostrar progresso a cada N blocos
}

// ============================================================================
// ESTRUTURAS DE DADOS COMUNS
// ============================================================================

/**
 * @brief Nó da árvore de Huffman para descompressão
 */
struct HuffmanNode {
    uint8_t symbol;                         // Símbolo (0-255) - válido apenas em folhas
    uint64_t frequency;                     // Frequência do símbolo
    std::shared_ptr<HuffmanNode> left;      // Filho esquerdo (bit 0)
    std::shared_ptr<HuffmanNode> right;     // Filho direito (bit 1)

    // Construtor para nó folha
    HuffmanNode(uint8_t sym, uint64_t freq) 
        : symbol(sym), frequency(freq), left(nullptr), right(nullptr) {}
    
    // Construtor para nó interno
    HuffmanNode(uint64_t freq, std::shared_ptr<HuffmanNode> l, std::shared_ptr<HuffmanNode> r) 
        : symbol(0), frequency(freq), left(l), right(r) {}
    
    bool isLeaf() const { return !left && !right; }
};

/**
 * @brief Comparador para priority_queue (min-heap por frequência)
 */
struct HuffmanNodeComparator {
    bool operator()(const std::shared_ptr<HuffmanNode>& a, 
                    const std::shared_ptr<HuffmanNode>& b) const {
        return a->frequency > b->frequency;
    }
};

// ============================================================================
// CLASSE: DESCODIFICADOR HUFFMAN (Modo Fast)
// ============================================================================

/**
 * @brief Descodificador Huffman que reconstrói a árvore a partir da tabela de frequências
 * 
 * Processo:
 * 1. Lê tabela de frequências serializada (1024 bytes)
 * 2. Reconstrói árvore de Huffman idêntica à do encoder
 * 3. Percorre bits e navega na árvore para descodificar símbolos
 */
class HuffmanDecoder {
public:
    /**
     * @brief Reconstrói a árvore de Huffman a partir da tabela de frequências
     * @param freqTableData Ponteiro para os 1024 bytes da tabela de frequências
     */
    void rebuildTree(const uint8_t* freqTableData) {
        // Interpretar bytes como array de uint32_t (little-endian)
        const uint32_t* frequencies = reinterpret_cast<const uint32_t*>(freqTableData);

        // Criar min-heap com nós folha (apenas símbolos com freq > 0)
        using MinHeap = std::priority_queue<
            std::shared_ptr<HuffmanNode>,
            std::vector<std::shared_ptr<HuffmanNode>>,
            HuffmanNodeComparator
        >;
        MinHeap heap;

        for (int i = 0; i < 256; ++i) {
            if (frequencies[i] > 0) {
                heap.push(std::make_shared<HuffmanNode>(
                    static_cast<uint8_t>(i), 
                    static_cast<uint64_t>(frequencies[i])
                ));
            }
        }

        // Casos especiais
        if (heap.empty()) {
            root = nullptr;
            return;
        }
        if (heap.size() == 1) {
            root = heap.top();
            return;
        }

        // Construir árvore combinando nós de menor frequência
        while (heap.size() > 1) {
            auto left = heap.top(); heap.pop();
            auto right = heap.top(); heap.pop();
            auto parent = std::make_shared<HuffmanNode>(
                left->frequency + right->frequency, left, right
            );
            heap.push(parent);
        }

        root = heap.top();
    }

    /**
     * @brief Descodifica stream de bits para símbolos originais
     * @param data Dados comprimidos (após tabela de frequências)
     * @param size Tamanho dos dados comprimidos em bytes
     * @param expectedSymbols Número de símbolos a descodificar
     * @return Vetor com símbolos descodificados
     */
    std::vector<uint8_t> decode(const uint8_t* data, size_t size, size_t expectedSymbols) {
        std::vector<uint8_t> output;
        output.reserve(expectedSymbols);

        if (!root) return output;

        // Caso especial: apenas 1 símbolo único (código é sempre "0")
        if (root->isLeaf()) {
            for (size_t i = 0; i < expectedSymbols; ++i) {
                output.push_back(root->symbol);
            }
            return output;
        }

        // Descodificação normal: navegar na árvore seguindo bits
        auto current = root;
        
        for (size_t byteIdx = 0; byteIdx < size && output.size() < expectedSymbols; ++byteIdx) {
            uint8_t byte = data[byteIdx];
            
            // Processar 8 bits (MSB primeiro)
            for (int bitIdx = 7; bitIdx >= 0 && output.size() < expectedSymbols; --bitIdx) {
                bool bit = (byte >> bitIdx) & 1;
                
                // Navegar na árvore
                current = bit ? current->right : current->left;

                // Chegou a uma folha: emitir símbolo e reiniciar
                if (current->isLeaf()) {
                    output.push_back(current->symbol);
                    current = root;
                }
            }
        }

        return output;
    }

private:
    std::shared_ptr<HuffmanNode> root;  // Raiz da árvore reconstruída
};

// ============================================================================
// CLASSE: DESCODIFICADOR ARITMÉTICO (Modo Best)
// ============================================================================

/**
 * @brief Descodificador aritmético com precisão de 32 bits
 * 
 * Processo inverso da codificação:
 * 1. Lê tabela de frequências e calcula cumulativas
 * 2. Mantém janela de 32 bits do stream comprimido
 * 3. Para cada símbolo: mapeia valor para símbolo, refina intervalo, renormaliza
 */
class ArithmeticDecoder {
    // Constantes idênticas ao encoder
    static constexpr uint64_t MAX_VALUE = 0xFFFFFFFF;
    static constexpr uint64_t ONE_QUARTER = 0x40000000;
    static constexpr uint64_t HALF = 0x80000000;
    static constexpr uint64_t THREE_QUARTERS = 0xC0000000;

public:
    ArithmeticDecoder() : cumulativeFreq(257, 0) {}

    /**
     * @brief Reconstrói modelo de probabilidades a partir da tabela
     * @param freqTableData Ponteiro para os 1024 bytes da tabela
     */
    void rebuildModel(const uint8_t* freqTableData) {
        const uint32_t* frequencies = reinterpret_cast<const uint32_t*>(freqTableData);
        
        // Calcular frequências cumulativas
        uint64_t total = 0;
        for (int i = 0; i < 256; ++i) {
            cumulativeFreq[i] = total;
            total += frequencies[i];
        }
        cumulativeFreq[256] = total;
    }

    /**
     * @brief Descodifica stream aritmético para símbolos originais
     * @param data Dados comprimidos (após tabela de frequências)
     * @param size Tamanho dos dados em bytes
     * @param expectedSymbols Número de símbolos a descodificar
     * @return Vetor com símbolos descodificados
     */
    std::vector<uint8_t> decode(const uint8_t* data, size_t size, size_t expectedSymbols) {
        std::vector<uint8_t> output;
        output.reserve(expectedSymbols);

        uint64_t low = 0;
        uint64_t high = MAX_VALUE;
        uint64_t value = 0;
        uint64_t totalCount = cumulativeFreq[256];

        // Estado do leitor de bits
        size_t byteIdx = 0;
        int bitIdx = 0;

        // Carregar primeiros 32 bits para o buffer de valor
        for (int i = 0; i < 32; ++i) {
            uint8_t bit = readBit(data, size, byteIdx, bitIdx);
            value = (value << 1) | bit;
        }

        // Descodificar cada símbolo
        for (size_t i = 0; i < expectedSymbols; ++i) {
            uint64_t range = high - low + 1;
            
            // Mapear valor para contagem cumulativa
            uint64_t scaledValue = ((value - low + 1) * totalCount - 1) / range;

            // Encontrar símbolo correspondente (busca linear - OK para 256 símbolos)
            int symbol = findSymbol(scaledValue);
            output.push_back(static_cast<uint8_t>(symbol));

            // Refinar intervalo (idêntico ao encoder)
            uint64_t symLow = cumulativeFreq[symbol];
            uint64_t symHigh = cumulativeFreq[symbol + 1];

            high = low + (range * symHigh) / totalCount - 1;
            low = low + (range * symLow) / totalCount;

            // Renormalização (idêntica ao encoder)
            while (true) {
                if (high < HALF) {
                    // Intervalo na metade inferior - nada a fazer
                } else if (low >= HALF) {
                    // Intervalo na metade superior
                    value -= HALF;
                    low -= HALF;
                    high -= HALF;
                } else if (low >= ONE_QUARTER && high < THREE_QUARTERS) {
                    // Intervalo no meio - expansão
                    value -= ONE_QUARTER;
                    low -= ONE_QUARTER;
                    high -= ONE_QUARTER;
                } else {
                    break;
                }
                
                // Shift e ler próximo bit
                low <<= 1;
                high = (high << 1) | 1;
                value = (value << 1) | readBit(data, size, byteIdx, bitIdx);
            }
        }

        return output;
    }

private:
    std::vector<uint64_t> cumulativeFreq;  // Frequências cumulativas [0..256]

    /**
     * @brief Lê um bit do stream comprimido
     */
    uint8_t readBit(const uint8_t* data, size_t size, size_t& byteIdx, int& bitIdx) {
        uint8_t bit = 0;
        if (byteIdx < size) {
            bit = (data[byteIdx] >> (7 - bitIdx)) & 1;
            bitIdx++;
            if (bitIdx == 8) {
                bitIdx = 0;
                byteIdx++;
            }
        }
        return bit;
    }

    /**
     * @brief Encontra símbolo correspondente a uma contagem cumulativa
     * @param scaledValue Valor escalado para o espaço de contagens
     * @return Índice do símbolo (0-255)
     */
    int findSymbol(uint64_t scaledValue) {
        // Busca binária: O(log 256) = 8 comparações em vez de 256
        int low = 0;
        int high = 255;
        
        while (low < high) {
            int mid = (low + high + 1) / 2;
            if (cumulativeFreq[mid] <= scaledValue) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        return low;
    }
};

// ============================================================================
// MÓDULO: DESCODIFICAÇÃO LSB
// ============================================================================

/**
 * @brief Descodifica dados LSB de acordo com o modo de compressão
 * 
 * Modo Fast: LSB são armazenados raw (sem processamento)
 * Modo Best: LSB podem estar em RLE ou Raw (indicado por flag)
 * 
 * Formato RLE:
 * - [flag=1] + dados RLE onde 0x00 é seguido de count
 * - [flag=0] + dados Raw
 * 
 * @param compressed Dados LSB comprimidos
 * @param isBestMode True se modo Best foi usado
 * @return Dados LSB originais
 */
std::vector<uint8_t> decodeLsb(const std::vector<uint8_t>& compressed, bool isBestMode) {
    if (compressed.empty()) return {};

    // Modo FAST: dados são raw puros (sem flag)
    if (!isBestMode) {
        return compressed;
    }

    // Modo BEST: primeiro byte é flag indicando formato
    uint8_t formatFlag = compressed[0];
    std::vector<uint8_t> output;

    if (formatFlag == 0) {
        // Flag 0: Fallback para Raw (RLE não compensava)
        output.insert(output.end(), compressed.begin() + 1, compressed.end());
    } 
    else if (formatFlag == 1) {
        // Flag 1: Dados em formato RLE
        output.reserve(compressed.size() * 2);  // Estimativa conservadora
        
        for (size_t i = 1; i < compressed.size(); ++i) {
            uint8_t byte = compressed[i];
            
            if (byte == 0x00) {
                // Escape: próximo byte indica quantos zeros
                if (i + 1 >= compressed.size()) break;
                
                uint8_t zeroCount = compressed[++i];
                for (int k = 0; k < zeroCount; ++k) {
                    output.push_back(0x00);
                }
            } else {
                // Byte literal
                output.push_back(byte);
            }
        }
    }

    return output;
}

// ============================================================================
// CLASSE: rANS DECODER (Modo Fast+ANS)
// ============================================================================

class RansDecoder {
public:
    static constexpr uint32_t RANS_L = 1u << 16;  // limite clássico (65536)

    std::vector<uint32_t> freq;      // 256 entradas
    std::vector<uint32_t> cumFreq;   // 257 entradas
    uint32_t total;                  // soma = 4096 (ou o que vier do encoder)

    struct DecodeEntry {
        uint8_t symbol;
        uint32_t freq;
        uint32_t start;
    };

    std::vector<DecodeEntry> decodeTable; // tabela de lookup com total entries

    // ---------------------------------------------------------
    // Reconstroi a tabela ANS a partir do header (1024 bytes)
    // ---------------------------------------------------------
    void rebuildModel(const uint8_t* freqTableData) {
        freq.resize(256);
        cumFreq.resize(257);

        const uint32_t* f = reinterpret_cast<const uint32_t*>(freqTableData);

        // Frequências do encoder
        total = 0;
        for (int i = 0; i < 256; ++i) {
            freq[i] = f[i];
            total += freq[i];
        }

        // Construir cumulativos
        uint32_t acc = 0;
        for (int i = 0; i < 256; i++) {
            cumFreq[i] = acc;
            acc += freq[i];
        }
        cumFreq[256] = acc;

        // Construir tabela de lookup (tamanho = total)
        decodeTable.resize(total);

        // Para cada símbolo, preencher o intervalo correspondente
        for (int s = 0; s < 256; ++s) {
            uint32_t begin = cumFreq[s];
            uint32_t end   = cumFreq[s] + freq[s];

            for (uint32_t i = begin; i < end; i++) {
                decodeTable[i].symbol = (uint8_t)s;
                decodeTable[i].freq   = freq[s];
                decodeTable[i].start  = cumFreq[s];
            }
        }
    }

    // ---------------------------------------------------------
    // Função de leitura do stream (little-endian)
    // ---------------------------------------------------------
    uint32_t read32(const uint8_t* d, size_t& idx) {
        uint32_t v = 
            (uint32_t)d[idx] |
            ((uint32_t)d[idx+1] << 8) |
            ((uint32_t)d[idx+2] << 16) |
            ((uint32_t)d[idx+3] << 24);
        idx += 4;
        return v;
    }

    // ---------------------------------------------------------
    // DESCODIFICAÇÃO rANS (reverse-order)
    // ---------------------------------------------------------
    std::vector<uint8_t> decode(const uint8_t* data, size_t size, size_t expectedSymbols) {
        std::vector<uint8_t> out(expectedSymbols);

        size_t idx = 0;

        // 1) Ler estado inicial
        uint32_t state = read32(data, idx);

        // 2) Começar pelos últimos símbolos (encoder codifica ao contrário)
        for (size_t pos = expectedSymbols; pos-- > 0;) {
            uint32_t slot = state % total;
            const DecodeEntry& e = decodeTable[slot];

            out[pos] = e.symbol;

            // Atualização ANS
            state = e.freq * (state / total) + (slot - e.start);

            // Refill enquanto o estado estiver demasiado baixo
            while (state < RANS_L) {
                if (idx >= size) break;
                state = (state << 8) | data[idx++];
            }
        }

        return out;
    }
};


// ============================================================================
// UTILITÁRIOS
// ============================================================================

/**
 * @brief Estrutura para estatísticas de descompressão
 */
struct DecompressionStats {
    uint64_t inputBytes = 0;
    uint64_t outputBytes = 0;
    int blocksProcessed = 0;
    
    double getExpansionRatio() const {
        return inputBytes > 0 ? static_cast<double>(outputBytes) / inputBytes : 0.0;
    }
    
    double getOutputSizeMB() const {
        return outputBytes / (1024.0 * 1024.0);
    }
};

/**
 * @brief Intercala bytes MSB e LSB para reconstruir valores 16-bit
 * @param msb Bytes mais significativos
 * @param lsb Bytes menos significativos
 * @return Buffer com valores 16-bit intercalados (little-endian)
 */
std::vector<char> mergeStreams(const std::vector<uint8_t>& msb, 
                                const std::vector<uint8_t>& lsb) {
    size_t numSamples = std::min(msb.size(), lsb.size());
    std::vector<char> output;
    output.reserve(numSamples * 2);

    for (size_t i = 0; i < numSamples; ++i) {
        output.push_back(static_cast<char>(lsb[i]));  // Byte baixo primeiro (little-endian)
        output.push_back(static_cast<char>(msb[i]));  // Byte alto
    }

    return output;
}

// ============================================================================
// FUNÇÃO PRINCIPAL
// ============================================================================

int main(int argc, char* argv[]) {
    // ========================================
    // PARSING DE ARGUMENTOS
    // ========================================
    if (argc < 3) {
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "  DECODER SafeTensors\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        std::cout << "  Uso: ./decoder <input.sc> <output.safetensors>\n";
        std::cout << "\n  Descomprime ficheiros .sc gerados pelo encoder.\n";
        std::cout << "  O modo (fast/best) é detetado automaticamente.\n";
        std::cout << "═══════════════════════════════════════════════════════\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    // Abrir ficheiros
    std::ifstream inputFile(inputPath, std::ios::binary);
    std::ofstream outputFile(outputPath, std::ios::binary);

    if (!inputFile) {
        std::cerr << "Erro: Não foi possível abrir " << inputPath << std::endl;
        return 1;
    }
    if (!outputFile) {
        std::cerr << "Erro: Não foi possível criar " << outputPath << std::endl;
        return 1;
    }

    auto startTime = std::chrono::high_resolution_clock::now();

    // ========================================
    // 1. PROCESSAR HEADER SAFETENSORS
    // ========================================
    uint64_t headerSize = 0;
    inputFile.read(reinterpret_cast<char*>(&headerSize), sizeof(headerSize));
    outputFile.write(reinterpret_cast<char*>(&headerSize), sizeof(headerSize));

    std::vector<char> headerJson(headerSize);
    inputFile.read(headerJson.data(), headerSize);
    outputFile.write(headerJson.data(), headerSize);

    // ========================================
    // 2. LER FLAG DE MODO GLOBAL
    // ========================================
    uint8_t modeFlag = 0;
    inputFile.read(reinterpret_cast<char*>(&modeFlag), 1);
    bool useBestMode = (modeFlag == 1);
    bool useRansMode = (modeFlag == 2);

    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  DECODER SafeTensors\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  Input:  " << inputPath << "\n";
    std::cout << "  Output: " << outputPath << "\n";
    std::cout << "  Modo:   " << (useRansMode ? "RANS (rANS + Raw)" : (useBestMode ? "BEST (Aritmética + RLE)" : "FAST (Huffman + Raw)")) << "\n";
    std::cout << "═══════════════════════════════════════════════════════\n";

    // ========================================
    // 3. LOOP DE DESCOMPRESSÃO POR BLOCOS
    // ========================================
    DecompressionStats stats;
    stats.inputBytes = 8 + headerSize + 1;  // Header já lido

    while (inputFile.peek() != EOF) {
        // A. Ler tamanhos do bloco
        uint32_t msbSize = 0, lsbSize = 0;
        inputFile.read(reinterpret_cast<char*>(&msbSize), 4);
        if (inputFile.eof()) break;
        inputFile.read(reinterpret_cast<char*>(&lsbSize), 4);

        // B. Ler dados comprimidos
        std::vector<uint8_t> msbCompressed(msbSize);
        std::vector<uint8_t> lsbCompressed(lsbSize);
        inputFile.read(reinterpret_cast<char*>(msbCompressed.data()), msbSize);
        inputFile.read(reinterpret_cast<char*>(lsbCompressed.data()), lsbSize);

        stats.inputBytes += 8 + msbSize + lsbSize;

        // C. Descodificar LSB (determina número de amostras)
        std::vector<uint8_t> lsbRaw = decodeLsb(lsbCompressed, useBestMode);
        size_t numSamples = lsbRaw.size();

        // D. Descodificar MSB
        std::vector<uint8_t> msbRaw;

        if (useBestMode) {
            ArithmeticDecoder decoder;
            decoder.rebuildModel(msbCompressed.data());
            msbRaw = decoder.decode(
                msbCompressed.data() + Config::FREQ_TABLE_SIZE,
                msbSize - Config::FREQ_TABLE_SIZE,
                numSamples
            );
        } else if (useRansMode) {
            RansDecoder decoder;
            decoder.rebuildModel(msbCompressed.data());
            msbRaw = decoder.decode(
                msbCompressed.data() + Config::FREQ_TABLE_SIZE,
                msbSize - Config::FREQ_TABLE_SIZE,
                numSamples
            );
        } else {
            HuffmanDecoder decoder;
            decoder.rebuildTree(msbCompressed.data());
            msbRaw = decoder.decode(
                msbCompressed.data() + Config::FREQ_TABLE_SIZE,
                msbSize - Config::FREQ_TABLE_SIZE,
                numSamples
            );
        }

        // E. Reconstruir e escrever dados originais
        std::vector<char> reconstructed = mergeStreams(msbRaw, lsbRaw);
        outputFile.write(reconstructed.data(), reconstructed.size());

        stats.outputBytes += reconstructed.size();
        stats.blocksProcessed++;

        // Mostrar progresso
        if (stats.blocksProcessed % Config::PROGRESS_INTERVAL == 0) {
            std::cout << "\r  Bloco " << stats.blocksProcessed 
                      << " | Restaurado: " << std::fixed << std::setprecision(1)
                      << stats.getOutputSizeMB() << " MB" << std::flush;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // ========================================
    // RELATÓRIO FINAL
    // ========================================
    std::cout << "\n═══════════════════════════════════════════════════════\n";
    std::cout << "  RESULTADO\n";
    std::cout << "═══════════════════════════════════════════════════════\n";
    std::cout << "  Blocos:     " << stats.blocksProcessed << "\n";
    std::cout << "  Entrada:    " << stats.inputBytes / (1024.0 * 1024.0) << " MB\n";
    std::cout << "  Saída:      " << stats.getOutputSizeMB() << " MB\n";
    std::cout << "  Expansão:   " << std::fixed << std::setprecision(3) 
              << stats.getExpansionRatio() << "x\n";
    std::cout << "  Tempo:      " << duration.count() / 1000.0 << " s\n";
    std::cout << "═══════════════════════════════════════════════════════\n";

    return 0;
}