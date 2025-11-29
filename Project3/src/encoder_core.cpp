#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <queue>
#include <map>
#include <memory>
#include <bitset>
#include <iomanip>


// Configurações
const size_t BLOCK_SIZE = 1024 * 1024; // 1 MB por bloco (ajustável para gestão de memória)


// ==========================================
// MÓDULO: CODIFICADOR ARITMÉTICO (Alternativa ao Passo C)
// Referência: ic-notas.pdf Seção 5.3
// ==========================================

class ArithmeticCodec {
    const uint64_t MAX_VAL = 0xFFFFFFFF;
    const uint64_t ONE_QUARTER = 0x40000000;
    const uint64_t HALF = 0x80000000;
    const uint64_t THREE_QUARTERS = 0xC0000000;

public:
    std::vector<uint32_t> frequencies;
    std::vector<uint64_t> cumulative_freq;

    ArithmeticCodec() : frequencies(256, 0), cumulative_freq(257, 0) {}

    void build(const std::vector<uint8_t>& data) {
        std::fill(frequencies.begin(), frequencies.end(), 0);
        // Garantir que nenhuma frequência é 0 (requisitado p/ aritmética)
        for (auto& f : frequencies) f = 1; 
        
        for (uint8_t b : data) frequencies[b]++;

        // Construir tabela cumulativa
        uint64_t total = 0;
        for (int i = 0; i < 256; ++i) {
            cumulative_freq[i] = total;
            total += frequencies[i];
        }
        cumulative_freq[256] = total;
    }

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> output;
        
        // 1. Escrever Tabela de Frequências (Overhead igual ao Huffman)
        for (uint32_t f : frequencies) {
            output.push_back(f & 0xFF);
            output.push_back((f >> 8) & 0xFF);
            output.push_back((f >> 16) & 0xFF);
            output.push_back((f >> 24) & 0xFF);
        }

        // 2. Processo de Codificação Aritmética
        uint64_t low = 0;
        uint64_t high = MAX_VAL;
        uint64_t pending_bits = 0;
        uint64_t total_count = cumulative_freq[256];

        for (uint8_t symbol : data) {
            uint64_t range = high - low + 1;
            uint64_t sym_low = cumulative_freq[symbol];
            uint64_t sym_high = cumulative_freq[symbol + 1];

            // Refinar o intervalo
            high = low + (range * sym_high) / total_count - 1;
            low = low + (range * sym_low) / total_count;

            // Renormalização (Output de bits)
            while (true) {
                if (high < HALF) {
                    output_bit_plus_pending(0, pending_bits, output);
                } else if (low >= HALF) {
                    output_bit_plus_pending(1, pending_bits, output);
                    low -= HALF;
                    high -= HALF;
                } else if (low >= ONE_QUARTER && high < THREE_QUARTERS) {
                    pending_bits++;
                    low -= ONE_QUARTER;
                    high -= ONE_QUARTER;
                } else {
                    break;
                }
                low <<= 1;
                high = (high << 1) | 1;
            }
        }
        
        // Finalizar bits pendentes
        pending_bits++;
        if (low < ONE_QUARTER) output_bit_plus_pending(0, pending_bits, output);
        else output_bit_plus_pending(1, pending_bits, output);

        return output;
    }

private:
    // Buffer de bits auxiliar
    uint8_t bit_buffer = 0;
    int bit_count = 0;

    void output_bit_plus_pending(int bit, uint64_t& pending, std::vector<uint8_t>& out) {
        write_bit(bit, out);
        while (pending > 0) {
            write_bit(!bit, out);
            pending--;
        }
    }

    void write_bit(int bit, std::vector<uint8_t>& out) {
        bit_buffer = (bit_buffer << 1) | bit;
        bit_count++;
        if (bit_count == 8) {
            out.push_back(bit_buffer);
            bit_count = 0;
            bit_buffer = 0;
        }
    }
};

// ==========================================
// MÓDULO AUXILIAR: ARVORE DE HUFFMAN (Passo C)
// Referência: ic-notas.pdf Seção 5.1.3
// ==========================================

struct Node {
    uint8_t symbol;
    uint64_t freq;
    std::shared_ptr<Node> left, right;

    Node(uint8_t s, uint64_t f) : symbol(s), freq(f), left(nullptr), right(nullptr) {}
    Node(uint64_t f, std::shared_ptr<Node> l, std::shared_ptr<Node> r) 
        : symbol(0), freq(f), left(l), right(r) {}
};

struct CompareNode {
    bool operator()(const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
        return a->freq > b->freq; // Min-heap (menor frequência no topo)
    }
};

class HuffmanCodec {
public:
    std::map<uint8_t, std::string> codes;
    std::vector<uint32_t> frequencies; // Usar 32 bits para contagem no bloco

    HuffmanCodec() : frequencies(256, 0) {}

    // 1. Construir Tabela de Frequências e Árvore
    void build(const std::vector<uint8_t>& data) {
        std::fill(frequencies.begin(), frequencies.end(), 0);
        for (uint8_t b : data) frequencies[b]++;

        std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, CompareNode> pq;

        for (int i = 0; i < 256; ++i) {
            if (frequencies[i] > 0) {
                pq.push(std::make_shared<Node>(static_cast<uint8_t>(i), frequencies[i]));
            }
        }

        // Caso degenerado: ficheiro vazio ou só 1 símbolo
        if (pq.empty()) return;
        if (pq.size() == 1) {
            codes[pq.top()->symbol] = "0";
            return;
        }

        while (pq.size() > 1) {
            auto left = pq.top(); pq.pop();
            auto right = pq.top(); pq.pop();
            auto parent = std::make_shared<Node>(left->freq + right->freq, left, right);
            pq.push(parent);
        }

        generate_codes(pq.top(), "");
    }

    // 2. Empacotar Bits (Encoding)
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> output;
        
        // A. Escrever Cabeçalho do Bloco: Tabela de Frequências (256 * 4 bytes = 1KB)
        // Necessário para o descodificador reconstruir a árvore
        for (uint32_t f : frequencies) {
            // Escrever em Little Endian
            output.push_back(f & 0xFF);
            output.push_back((f >> 8) & 0xFF);
            output.push_back((f >> 16) & 0xFF);
            output.push_back((f >> 24) & 0xFF);
        }

        // B. Escrever Dados Comprimidos (Bitstream)
        uint8_t accumulator = 0;
        int bit_count = 0;

        for (uint8_t symbol : data) {
            const std::string& code = codes[symbol];
            for (char bit : code) {
                accumulator = (accumulator << 1) | (bit - '0');
                bit_count++;
                if (bit_count == 8) {
                    output.push_back(accumulator);
                    accumulator = 0;
                    bit_count = 0;
                }
            }
        }

        // C. Padding do último byte (se sobrar bits)
        // Nota: Um formato robusto guardaria o número de bits válidos, 
        // mas para este trabalho assumimos padding com zeros.
        if (bit_count > 0) {
            accumulator = accumulator << (8 - bit_count);
            output.push_back(accumulator);
        }

        return output;
    }

private:
    void generate_codes(std::shared_ptr<Node> node, std::string code) {
        if (!node) return;
        if (!node->left && !node->right) {
            codes[node->symbol] = code;
        }
        generate_codes(node->left, code + "0");
        generate_codes(node->right, code + "1");
    }
};


// ==========================================
// FUNÇÕES DE UTILIDADE (Cálculo Entropia)
// ==========================================

double calculate_vector_entropy(const std::vector<uint8_t>& data) {
    if (data.empty()) return 0.0;
    std::vector<uint64_t> counts(256, 0);
    for (uint8_t b : data) counts[b]++;
    
    double entropy = 0.0;
    for (auto c : counts) {
        if (c > 0) {
            double p = (double)c / data.size();
            entropy -= p * std::log2(p);
        }
    }
    return entropy;
}



// --- MÓDULO 3.2: MSB COMPRESSION ---

// PASSO A: Aplica Predição Delta: r[i] = x[i] - x[i-1]
// Retorna o vetor de resíduos (erros de predição)
/*std::vector<uint8_t> apply_delta_prediction(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> residuals;
    residuals.reserve(data.size());
    uint8_t prev = 0; 
    for (uint8_t byte : data) {
        uint8_t delta = byte - prev; 
        residuals.push_back(delta);
        prev = byte; 
    }
    return residuals;
}*/

// PASSO A: Aplica Predição Delta com XOR
// Retorna o vetor de resíduos (erros de predição)
/*std::vector<uint8_t> apply_delta_prediction(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> residuals;
    residuals.reserve(data.size());
    
    uint8_t prev = 0; 
    for (uint8_t byte : data) {
        // TENTATIVA 2: XOR em vez de Subtração
        // O XOR remove a parte "igual" do byte anterior sem sofrer com o sinal
        uint8_t delta = byte ^ prev; 
        
        residuals.push_back(delta);
        prev = byte; 
    }
    return residuals;
}*/

// PASSO C: Codificador de Entropia (Huffman)
/*std::vector<uint8_t> entropy_encode_msb(const std::vector<uint8_t>& residuals) {
    HuffmanCodec huffman;
    
    // 1. Construir modelo estatístico do bloco
    huffman.build(residuals);
    
    // 2. Gerar bitstream
    return huffman.compress(residuals);
}*/

// PASSO C: Codificador de Entropia (Aritmético)
std::vector<uint8_t> entropy_encode_msb(const std::vector<uint8_t>& residuals) {
    ArithmeticCodec ac;

    // 1. Construir modelo estatístico do bloco
    ac.build(residuals);

    // 2. Gerar bitstream
    return ac.compress(residuals);
}

// --- MÓDULO 3.3: LSB COMPRESSION ---

// Implementar estratégia LSB (Raw ou LZ4)
std::vector<uint8_t> encode_lsb(const std::vector<uint8_t>& data) {
    // POR ENQUANTO: Raw copy (estratégia base definida no relatório)
    return data; 
}

// --- MÓDULO 3.1: SPLITTER & CORE LOOP ---

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Uso: ./encoder_core <input.safetensors> <output.sc>" << std::endl;
        return 1;
    }

    std::string input_path = argv[1];
    std::string output_path = argv[2];

    std::ifstream in_file(input_path, std::ios::binary);
    std::ofstream out_file(output_path, std::ios::binary);

    if (!in_file || !out_file) {
        std::cerr << "Erro ao abrir ficheiros." << std::endl;
        return 1;
    }

    // 1. Processar Cabeçalho (Copiar ipsis verbis)
    uint64_t header_size = 0;
    in_file.read(reinterpret_cast<char*>(&header_size), sizeof(header_size));
    
    // Escrever tamanho do header
    out_file.write(reinterpret_cast<char*>(&header_size), sizeof(header_size));
    
    // Copiar o JSON
    std::vector<char> header_json(header_size);
    in_file.read(header_json.data(), header_size);
    out_file.write(header_json.data(), header_size);

    std::cout << "Header copiado (" << header_size << " bytes). Iniciando compressao de blocos..." << std::endl;

    // 2. Loop de Processamento por Blocos
    std::vector<char> buffer(BLOCK_SIZE);
    std::vector<uint8_t> msb_channel;
    std::vector<uint8_t> lsb_channel;
    
    // Reservar memória para evitar realocações constantes
    msb_channel.reserve(BLOCK_SIZE / 2);
    lsb_channel.reserve(BLOCK_SIZE / 2);

    uint64_t total_bytes_in = 0;
    uint64_t total_bytes_out = 0;

    // Médias para relatório
    double total_entropy_msb_raw = 0;
    double total_entropy_msb_delta = 0;
    int blocks_processed = 0;

    //std::cout << "Iniciando Compressao Hibrida (Split -> Huffman)..." << std::endl;
    std::cout << "Iniciando Compressao Hibrida (Split -> Aritmetico)..." << std::endl;

    while (in_file.read(buffer.data(), BLOCK_SIZE) || in_file.gcount() > 0) {
        size_t bytes_read = in_file.gcount();
        size_t pairs = bytes_read / 2;

        msb_channel.clear();
        lsb_channel.clear();

        // --- PASSO 3.1: SPLITTER ---
        for (size_t i = 0; i < pairs * 2; i += 2) {
            // BF16 Little Endian: [Byte 0: LSB/Mantissa] [Byte 1: MSB/Expoente]
            lsb_channel.push_back((uint8_t)buffer[i]);
            msb_channel.push_back((uint8_t)buffer[i+1]);
        }

        // Métricas antes (para o relatório/debug)
        double ent_raw = calculate_vector_entropy(msb_channel);

        // --- PASSO 3.2: PROCESSAR MSB --
        // A. Predição Delta
        // std::vector<uint8_t> msb_residuals = apply_delta_prediction(msb_channel);
        // B: Verificação (Métrica pós-transformação)
        //double ent_delta = calculate_vector_entropy(msb_residuals);
        // C. Codificação de Entropia
        //std::vector<uint8_t> msb_encoded = entropy_encode_msb(msb_residuals);

        // --- PASSO 3.2: PROCESSAR MSB (MUDANÇA DE ESTRATÉGIA) ---
        // 1. Decisão baseada em dados: Predição (Delta/XOR) aumentou entropia.
        // A melhor estratégia é usar a distribuição original (bimodal).
        std::vector<uint8_t> msb_encoded = entropy_encode_msb(msb_channel);

        // Acumular estatísticas
        total_entropy_msb_raw += ent_raw;
        //total_entropy_msb_delta += ent_delta;
        total_entropy_msb_delta += ent_raw; // Usando raw como baseline devido à mudança de estratégia
        blocks_processed++;

        // --- PASSO 3.3: PROCESSAR LSB ---
        std::vector<uint8_t> lsb_encoded = encode_lsb(lsb_channel);

        // --- PASSO 3.4: EMPACOTAMENTO (Simples Sequencial) ---
        // Aqui podesse definir um formato mais robusto.
        // Por agora: Escrevemos MSB comprimido seguido de LSB.
        out_file.write(reinterpret_cast<char*>(msb_encoded.data()), msb_encoded.size());
        out_file.write(reinterpret_cast<char*>(lsb_encoded.data()), lsb_encoded.size());

        total_bytes_in += bytes_read;
        total_bytes_out += (msb_encoded.size() + lsb_encoded.size());
        
        //std::cout << "\rProcessado: " << (total_bytes_in / (1024*1024)) << " MB" << std::flush;
        // Log de progresso com métricas de entropia
        if (blocks_processed % 10 == 0) {
            std::cout << "\r[Bloco " << blocks_processed << "] "
                      //<< "Entropia MSB: " << std::fixed << std::setprecision(2)
                      //<< ent_raw << " -> " << ent_delta << " bits/byte. "
                      << "Ratio atual: " << std::fixed << std::setprecision(2)
                      << (double)total_bytes_in / total_bytes_out << ":1" << std::flush;
    
        }
    }

    std::cout << "\n\n--- Relatorio Final do Encoder Core ---" << std::endl;
    std::cout << "Entropia Media MSB Original: " << total_entropy_msb_raw / blocks_processed << std::endl;
    std::cout << "Entropia Media MSB Residual (Delta): " << total_entropy_msb_delta / blocks_processed << std::endl;
    std::cout << "Ganho de Entropia: " << (total_entropy_msb_raw - total_entropy_msb_delta) / blocks_processed << " bits" << std::endl;
    std::cout << "Tamanho Final: " << total_bytes_out / (1024.0*1024.0) << " MB" << std::endl;
    return 0;
}