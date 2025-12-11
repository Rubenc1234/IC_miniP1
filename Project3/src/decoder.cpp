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

// ==========================================
// DECODER HUFFMAN (Inverso do Fast)
// ==========================================
struct Node {
    uint8_t symbol;
    std::shared_ptr<Node> left, right;
    Node(uint8_t s) : symbol(s), left(nullptr), right(nullptr) {}
    Node(std::shared_ptr<Node> l, std::shared_ptr<Node> r) : symbol(0), left(l), right(r) {}
};

class HuffmanDecoder {
    std::shared_ptr<Node> root;
public:
    void rebuild(const std::vector<uint32_t>& frequencies) {
        struct PNode {
            uint64_t freq;
            std::shared_ptr<Node> node;
            bool operator>(const PNode& other) const { return freq > other.freq; }
        };
        std::priority_queue<PNode, std::vector<PNode>, std::greater<PNode>> pq;

        for (int i = 0; i < 256; ++i) {
            if (frequencies[i] > 0) pq.push({(uint64_t)frequencies[i], std::make_shared<Node>((uint8_t)i)});
        }

        if (pq.empty()) return;
        if (pq.size() == 1) {
            root = std::make_shared<Node>(nullptr, nullptr); 
            root->left = pq.top().node; 
            return;
        }

        while (pq.size() > 1) {
            auto l = pq.top(); pq.pop();
            auto r = pq.top(); pq.pop();
            auto parent = std::make_shared<Node>(l.node, r.node);
            pq.push({l.freq + r.freq, parent});
        }
        root = pq.top().node;
    }

    void decompress(const std::vector<uint8_t>& in_bits, std::vector<uint8_t>& out_data, size_t original_size) {
        out_data.clear(); out_data.reserve(original_size);
        auto current = root;
        size_t count = 0;
        
        if (!root) return; // Bloco vazio
        
        // Caso especial: apenas 1 símbolo repetido
        if (!current->left && !current->right) {
             // Lógica: Se a árvore só tem 1 nó, o código deve ser tamanho 1 (ex: '0')
             // A tua implementação do Encoder assume table[sym] = {0, 1}
             // O bitstream terá N zeros.
             // Simplesmente preenchemos com o símbolo.
             if (current->left) out_data.assign(original_size, current->left->symbol); // Hack p/ estrutura
             return; 
        }

        for (uint8_t byte : in_bits) {
            for (int i = 7; i >= 0; --i) {
                if (count >= original_size) return;
                bool bit = (byte >> i) & 1;
                if (bit == 0) current = current->left; else current = current->right;

                if (!current->left && !current->right) { 
                    out_data.push_back(current->symbol);
                    current = root;
                    count++;
                }
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