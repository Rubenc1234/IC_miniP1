#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cmath>
#include <queue>
#include <memory>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <functional>

// ==========================================
// DECODER HUFFMAN OTIMIZADO COM LUT
// ==========================================
// Usa tabela de lookup para descodificar até 12 bits de uma vez

class HuffmanDecoder {
    static constexpr int LUT_BITS = 12;  // Tamanho da LUT (4096 entradas)
    static constexpr int LUT_SIZE = 1 << LUT_BITS;
    
    struct LUTEntry {
        uint8_t symbol;   // Símbolo descodificado
        uint8_t bits;     // Número de bits consumidos (0 = código mais longo que LUT_BITS)
    };
    
    std::vector<LUTEntry> lut;
    
    // Para códigos mais longos que LUT_BITS, usamos árvore tradicional
    struct Node {
        uint8_t symbol;
        bool is_leaf;
        int left, right;  // Índices na pool de nós
    };
    std::vector<Node> nodes;
    int root_idx;
    bool single_symbol;
    uint8_t single_sym;

public:
    HuffmanDecoder() : lut(LUT_SIZE), single_symbol(false) {}

    void rebuild(const std::vector<uint32_t>& frequencies) {
        // 1. Construir árvore de Huffman e extrair códigos
        struct BuildNode {
            uint64_t freq;
            int idx;
            bool operator>(const BuildNode& o) const { return freq > o.freq; }
        };
        
        nodes.clear();
        std::priority_queue<BuildNode, std::vector<BuildNode>, std::greater<BuildNode>> pq;
        
        // Criar nós folha
        for (int i = 0; i < 256; ++i) {
            if (frequencies[i] > 0) {
                int idx = nodes.size();
                nodes.push_back({(uint8_t)i, true, -1, -1});
                pq.push({frequencies[i], idx});
            }
        }
        
        if (pq.empty()) { root_idx = -1; return; }
        
        // Caso especial: único símbolo
        if (pq.size() == 1) {
            single_symbol = true;
            single_sym = nodes[pq.top().idx].symbol;
            root_idx = pq.top().idx;
            return;
        }
        single_symbol = false;
        
        // Construir árvore
        while (pq.size() > 1) {
            auto l = pq.top(); pq.pop();
            auto r = pq.top(); pq.pop();
            int idx = nodes.size();
            nodes.push_back({0, false, l.idx, r.idx});
            pq.push({l.freq + r.freq, idx});
        }
        root_idx = pq.top().idx;
        
        // 2. Gerar códigos canónicos
        struct CodeInfo { uint8_t symbol; uint32_t code; uint8_t len; };
        std::vector<CodeInfo> codes;
        
        // DFS para extrair códigos
        std::function<void(int, uint32_t, uint8_t)> extract = [&](int idx, uint32_t code, uint8_t len) {
            if (idx < 0) return;
            const Node& n = nodes[idx];
            if (n.is_leaf) {
                codes.push_back({n.symbol, code, len});
                return;
            }
            extract(n.left, code << 1, len + 1);
            extract(n.right, (code << 1) | 1, len + 1);
        };
        extract(root_idx, 0, 0);
        
        // 3. Preencher LUT
        for (auto& e : lut) { e.symbol = 0; e.bits = 0; }
        
        for (const auto& c : codes) {
            if (c.len <= LUT_BITS) {
                // Preencher todas as entradas que começam com este código
                int prefix = c.code << (LUT_BITS - c.len);
                int count = 1 << (LUT_BITS - c.len);
                for (int i = 0; i < count; ++i) {
                    lut[prefix + i] = {c.symbol, c.len};
                }
            }
        }
    }

    void decompress(const std::vector<uint8_t>& in_bits, std::vector<uint8_t>& out_data, size_t original_size) {
        out_data.clear();
        out_data.resize(original_size);
        
        if (original_size == 0 || root_idx < 0) return;
        
        // Caso especial: único símbolo
        if (single_symbol) {
            std::fill(out_data.begin(), out_data.end(), single_sym);
            return;
        }
        
        // Buffer de bits para leitura rápida
        uint64_t bit_buf = 0;
        int bits_in_buf = 0;
        size_t byte_pos = 0;
        size_t out_pos = 0;
        
        // Pré-carregar buffer
        while (bits_in_buf <= 56 && byte_pos < in_bits.size()) {
            bit_buf |= (uint64_t)in_bits[byte_pos++] << (56 - bits_in_buf);
            bits_in_buf += 8;
        }
        
        while (out_pos < original_size) {
            // Extrair LUT_BITS do topo do buffer
            uint32_t lookup = (bit_buf >> (64 - LUT_BITS)) & (LUT_SIZE - 1);
            const LUTEntry& entry = lut[lookup];
            
            if (entry.bits > 0) {
                // Descodificação rápida via LUT
                out_data[out_pos++] = entry.symbol;
                bit_buf <<= entry.bits;
                bits_in_buf -= entry.bits;
            } else {
                // Código mais longo que LUT_BITS - fallback para travessia
                int idx = root_idx;
                while (!nodes[idx].is_leaf) {
                    bool bit = (bit_buf >> 63) & 1;
                    bit_buf <<= 1;
                    bits_in_buf--;
                    idx = bit ? nodes[idx].right : nodes[idx].left;
                    
                    // Recarregar se necessário
                    if (bits_in_buf < 16 && byte_pos < in_bits.size()) {
                        while (bits_in_buf <= 56 && byte_pos < in_bits.size()) {
                            bit_buf |= (uint64_t)in_bits[byte_pos++] << (56 - bits_in_buf);
                            bits_in_buf += 8;
                        }
                    }
                }
                out_data[out_pos++] = nodes[idx].symbol;
            }
            
            // Recarregar buffer
            while (bits_in_buf <= 56 && byte_pos < in_bits.size()) {
                bit_buf |= (uint64_t)in_bits[byte_pos++] << (56 - bits_in_buf);
                bits_in_buf += 8;
            }
        }
    }
};

// ==========================================
// DECODER rANS (Inverso do Best)
// ==========================================
class ANSDecoder {
    static constexpr uint32_t TOT = 1u << 12;
    static constexpr uint32_t SHIFT = 12;
    static constexpr uint32_t MASK = TOT - 1;

    std::vector<uint32_t> freq;
    std::vector<uint32_t> norm_freq;
    std::vector<uint32_t> cumul;
    std::vector<uint8_t> symtab;

public:
    void rebuild(const std::vector<uint32_t>& in_freq) {
        freq = in_freq; // Copiar frequências lidas
        normalize_freq();
        build_cumul_and_symtab();
    }

    void decompress(const std::vector<uint8_t>& payload, std::vector<uint8_t>& out_data, size_t original_size) {
        out_data.clear(); out_data.resize(original_size);
        
        if (payload.empty() || original_size == 0) return;
        
        size_t ptr = 0;
        uint64_t state = 0;
        
        // O encoder escreveu o estado final byte a byte (LSB first) e reverteu tudo.
        // Após reverse, os bytes do estado final estão no início do payload (MSB first).
        // Precisamos ler bytes até termos um estado válido >= 2^32
        
        // Ler os primeiros bytes para formar o estado inicial
        // O estado foi escrito com while(state>0) push LSB, então pode ter 5-8 bytes
        // Após reverse, lemos MSB first
        int state_bytes = 0;
        while (ptr < payload.size() && state_bytes < 8) {
            state = (state << 8) | payload[ptr++];
            state_bytes++;
            if (state >= (1ULL << 32)) break;
        }
        
        // Decode symbols in forward order
        for (size_t i = 0; i < original_size; ++i) {
            // 1. Find symbol from slot
            uint32_t slot = state & MASK;
            uint8_t s = symtab[slot];
            out_data[i] = s;
            
            // 2. Update state (inverse of encode step)
            uint32_t f = norm_freq[s];
            uint32_t start = cumul[s];
            state = (uint64_t)f * (state >> SHIFT) + (slot - start);
            
            // 3. Renormalize - read bytes while state is too small
            while (state < (1ULL << 32) && ptr < payload.size()) {
                state = (state << 8) | payload[ptr++];
            }
        }
    }

private:
    // Copiar a mesma lógica de normalização do Encoder para garantir simetria
    void normalize_freq() {
        norm_freq.assign(256, 0);
        uint64_t total = 0;
        for (int i = 0; i < 256; ++i) total += freq[i];
        if (total == 0) { norm_freq[0] = TOT; return; }

        double scale = static_cast<double>(TOT) / static_cast<double>(total);
        uint32_t sum = 0;
        for (int i = 0; i < 256; ++i) {
            if (freq[i] == 0) continue;
            uint32_t v = static_cast<uint32_t>(std::floor(freq[i] * scale));
            if (v == 0) v = 1;
            norm_freq[i] = v;
            sum += v;
        }

        if (sum < TOT) {
            std::vector<int> idx(256);
            for (int i=0; i<256; ++i) idx[i]=i;
            std::sort(idx.begin(), idx.end(), [&](int a, int b){ return freq[a] > freq[b]; });
            size_t p=0;
            while(sum < TOT) { 
                int s=idx[p%256]; if(freq[s]>0) { norm_freq[s]++; sum++; } 
                p++; 
            }
        } else if (sum > TOT) {
            std::vector<int> idx(256);
            for (int i=0; i<256; ++i) idx[i]=i;
            std::sort(idx.begin(), idx.end(), [&](int a, int b){ return freq[a] < freq[b]; });
            size_t p=0;
            while(sum > TOT) {
                int s=idx[p%256]; if(norm_freq[s]>1) { norm_freq[s]--; sum--; }
                p++;
            }
        }
    }

    void build_cumul_and_symtab() {
        cumul.assign(257, 0);
        for (int i = 0; i < 256; ++i) cumul[i+1] = cumul[i] + norm_freq[i];
        symtab.assign(TOT, 0);
        for (int s = 0; s < 256; ++s) {
            for (uint32_t pos = cumul[s]; pos < cumul[s+1]; ++pos) symtab[pos] = static_cast<uint8_t>(s);
        }
    }
};

// ==========================================
// MAIN
// ==========================================
int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false); // Otimização IO

    if (argc < 3) {
        std::cout << "Uso: ./decoder <input.sc> <output.safetensors>" << std::endl;
        return 1;
    }
    std::string in_p = argv[1], out_p = argv[2];

    std::ifstream in(in_p, std::ios::binary); 
    std::ofstream out(out_p, std::ios::binary);
    if (!in || !out) return 1;

    // 1. Ler Header Global
    uint64_t h_sz = 0; in.read((char*)&h_sz, 8); out.write((char*)&h_sz, 8);
    std::vector<char> h_json(h_sz); in.read(h_json.data(), h_sz); out.write(h_json.data(), h_sz);

    // 2. Ler Flag de Modo
    uint8_t mode_flag = 0;
    in.read((char*)&mode_flag, 1);
    bool use_best = (mode_flag == 1);
    
    std::cout << "A descodificar... Modo: " << (use_best ? "BEST (rANS)" : "FAST (Huffman)") << std::endl;

    HuffmanDecoder hd;
    ANSDecoder ans;

    // 3. Loop de Blocos
    while (in.peek() != EOF) {
        uint32_t sz_m = 0, sz_l = 0;
        in.read((char*)&sz_m, 4);
        if (in.eof()) break; 
        in.read((char*)&sz_l, 4);

        // Ler MSB e LSB
        std::vector<uint8_t> msb_packet(sz_m), lsb(sz_l);
        in.read((char*)msb_packet.data(), sz_m);
        in.read((char*)lsb.data(), sz_l);

        // --- Recuperar Tabela de Frequências (1024 bytes) ---
        if (sz_m < 1024) { std::cerr << "Erro: Bloco inválido." << std::endl; return 1; }
        std::vector<uint32_t> freqs(256);
        // O encoder escreveu uint32 em little-endian. Em x86 é direto.
        memcpy(freqs.data(), msb_packet.data(), 1024);
        
        // Payload começa após os 1024 bytes
        std::vector<uint8_t> payload(msb_packet.begin() + 1024, msb_packet.end());
        std::vector<uint8_t> msb;

        // --- Descompressão MSB ---
        if (use_best) {
            ans.rebuild(freqs);
            ans.decompress(payload, msb, lsb.size()); // Size MSB == Size LSB
        } else {
            hd.rebuild(freqs);
            hd.decompress(payload, msb, lsb.size());
        }

        // --- Merge (LSB, MSB, LSB, MSB...) ---
        std::vector<char> buffer;
        buffer.reserve(lsb.size() * 2);
        for (size_t i = 0; i < lsb.size(); ++i) {
            buffer.push_back((char)lsb[i]);
            buffer.push_back((char)msb[i]);
        }
        out.write(buffer.data(), buffer.size());
    }

    std::cout << "Concluído." << std::endl;
    return 0;
}