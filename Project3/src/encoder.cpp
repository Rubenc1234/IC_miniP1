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
#include <string>
#include <cstring>
#include <algorithm>

// Configurações
const size_t BLOCK_SIZE = 1024 * 1024; // 1 MB por bloco

// ==========================================
// MÓDULO 1: HUFFMAN (modo Fast)atual
// ==========================================
// (Mantive a tua versão LUT que já é ótima: ~3.0s, ~1.49:1)
struct Node {
    uint8_t symbol;
    uint64_t freq;
    std::shared_ptr<Node> left, right;
    Node(uint8_t s, uint64_t f) : symbol(s), freq(f) {}
    Node(uint64_t f, std::shared_ptr<Node> l, std::shared_ptr<Node> r) : symbol(0), freq(f), left(l), right(r) {}
};
struct CompareNode {
    bool operator()(const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) { return a->freq > b->freq; }
};

class HuffmanCodec {
    struct Code { uint32_t bits; uint8_t len; };
    Code table[256];
public:
    std::vector<uint32_t> frequencies;
    HuffmanCodec() : frequencies(256, 0) {}

    void build(const std::vector<uint8_t>& data) {
        std::fill(frequencies.begin(), frequencies.end(), 0);
        for (uint8_t b : data) frequencies[b]++;
        
        std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, CompareNode> pq;
        for (int i = 0; i < 256; ++i) if (frequencies[i] > 0) pq.push(std::make_shared<Node>((uint8_t)i, frequencies[i]));
        
        if (pq.empty()) return;
        if (pq.size() == 1) { table[pq.top()->symbol] = {0, 1}; return; }
        
        while (pq.size() > 1) {
            auto l = pq.top(); pq.pop(); auto r = pq.top(); pq.pop();
            pq.push(std::make_shared<Node>(l->freq + r->freq, l, r));
        }
        gen_lut(pq.top(), 0, 0);
    }

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> out; out.reserve(data.size());
        for (uint32_t f : frequencies) {
            out.push_back(f & 0xFF); out.push_back((f >> 8) & 0xFF);
            out.push_back((f >> 16) & 0xFF); out.push_back((f >> 24) & 0xFF);
        }
        uint64_t buf = 0; int bc = 0;
        for (uint8_t s : data) {
            Code c = table[s];
            buf = (buf << c.len) | c.bits; bc += c.len;
            while (bc >= 8) { bc -= 8; out.push_back((buf >> bc) & 0xFF); }
        }
        if (bc > 0) out.push_back((buf << (8 - bc)) & 0xFF);
        return out;
    }
private:
    void gen_lut(std::shared_ptr<Node> n, uint32_t c, uint8_t l) {
        if (!n) return;
        if (!n->left && !n->right) { table[n->symbol] = {c, l}; return; }
        gen_lut(n->left, c << 1, l + 1); gen_lut(n->right, (c << 1) | 1, l + 1);
    }
};

// ==========================================
// MÓDULO MSB: rANS (modo Best)
// ==========================================
class AssymetricalNumericalSystem {
public:
    // Parâmetros do codificador
    static constexpr uint32_t TOT = 1u << 12; // 4096
    static constexpr uint32_t SHIFT = 12;     // log2(TOT)

    // Tabelas construídas em build()
    std::vector<uint32_t> freq;       // frequências originais (32-bit)
    std::vector<uint32_t> norm_freq;  // frequências normalizadas (somam TOT)
    std::vector<uint32_t> cumul;      // cumulativas (size 257)
    std::vector<uint8_t> symtab;      // tabela de símbolo para decoder (size TOT)

    AssymetricalNumericalSystem() {}

    // Constrói as tabelas a partir dos dados (histograma)
    void build(const std::vector<uint8_t>& data) {
        // Conta frequências originais (32-bit)
        freq.assign(256, 0);
        for (uint8_t b : data) freq[b]++;

        // Normalizar freq -> norm_freq com soma EXACTA = TOT
        normalize_freq();
        // Construir cumulativas e symtab para decoder
        build_cumul_and_symtab();
    }

    // Compress: retorna buffer com [freq_table(256*4 bytes)] + [rANS payload]
    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> out;

        // 1) Serializar freq original (256 * 4 bytes little-endian) para compatibilidade
        for (int i = 0; i < 256; ++i) {
            uint32_t f = freq[i];
            out.push_back(static_cast<uint8_t>(f & 0xFF));
            out.push_back(static_cast<uint8_t>((f >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>((f >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((f >> 24) & 0xFF));
        }

        // 2) rANS encode (bytewise, reverse order)
        std::vector<uint8_t> payload;
        payload.reserve(data.size() / 2); // heuristic

        // Initial state: use a 64-bit state.
        uint64_t state = (1ULL << 32); // initial large state >= 2^32

        // Encode symbols in reverse order
        for (int idx = (int)data.size() - 1; idx >= 0; --idx) {
            uint8_t s = data[idx];
            uint32_t f = norm_freq[s];
            uint32_t start = cumul[s];

            // Renormalize: emit bytes while state is too large for this freq
            // Condition chosen empirically: while state >= (f << 32)
            // (keeps arithmetic safe and ensures decoder can renormalize)
            while (state >= ( (uint64_t)f << 32 )) {
                payload.push_back(static_cast<uint8_t>(state & 0xFF));
                state >>= 8;
            }

            // rANS encode step (bytewise variant)
            uint64_t q = state / f;
            uint64_t r = state % f;
            state = (q << SHIFT) + r + start;
        }

        // Flush remaining state bytes (LSB-first)
        while (state > 0) {
            payload.push_back(static_cast<uint8_t>(state & 0xFF));
            state >>= 8;
        }

        // rANS writes backwards, so reverse payload bytes
        std::reverse(payload.begin(), payload.end());

        // 3) Append payload to out and return
        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

private:
    // Normalização simples: escala e depois corrige para somar exatamente TOT
    void normalize_freq() {
        norm_freq.assign(256, 0);

        // Total real
        uint64_t total = 0;
        for (int i = 0; i < 256; ++i) total += freq[i];
        if (total == 0) {
            // Bloco vazio: tudo zero (evitar div by zero)
            // dar 1 a um símbolo arbitrário para estabilidade
            norm_freq[0] = TOT;
            return;
        }

        // Scaling (floor) e garantir pelo menos 1 para símbolos presentes
        double scale = static_cast<double>(TOT) / static_cast<double>(total);
        uint32_t sum = 0;
        for (int i = 0; i < 256; ++i) {
            if (freq[i] == 0) { norm_freq[i] = 0; continue; }
            uint32_t v = static_cast<uint32_t>(std::floor(freq[i] * scale));
            if (v == 0) v = 1;
            norm_freq[i] = v;
            sum += v;
        }

        // Corrigir diferenças devido a arredondamento
        // Se soma menor que TOT, incrementa símbolos com maior original freq
        if (sum < TOT) {
            // Build array of indices sorted by original freq desc
            std::vector<int> idx(256);
            for (int i = 0; i < 256; ++i) idx[i] = i;
            std::sort(idx.begin(), idx.end(), [&](int a, int b){ return freq[a] > freq[b]; });
            size_t p = 0;
            while (sum < TOT) {
                int s = idx[p % 256];
                // preferir símbolos que existem (freq>0)
                if (freq[s] > 0) { norm_freq[s]++; sum++; }
                p++;
            }
        } else if (sum > TOT) {
            // reduzir símbolos com menor impacto (freq small)
            std::vector<int> idx(256);
            for (int i = 0; i < 256; ++i) idx[i] = i;
            std::sort(idx.begin(), idx.end(), [&](int a, int b){ return freq[a] < freq[b]; });
            size_t p = 0;
            while (sum > TOT) {
                int s = idx[p % 256];
                if (norm_freq[s] > 1) { norm_freq[s]--; sum--; }
                p++;
            }
        }
        // agora sum == TOT
    }

    void build_cumul_and_symtab() {
        cumul.assign(257, 0);
        for (int i = 0; i < 256; ++i) cumul[i+1] = cumul[i] + norm_freq[i];
        symtab.assign(TOT, 0);
        for (int s = 0; s < 256; ++s) {
            for (uint32_t pos = cumul[s]; pos < cumul[s+1]; ++pos) {
                symtab[pos] = static_cast<uint8_t>(s);
            }
        }
    }
};

// ==========================================
// MAIN LOOP
// ==========================================
int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Uso: ./encoder_core <input> <output> [mode: fast|best]" << std::endl;
        return 1;
    }
    std::string in_p = argv[1], out_p = argv[2], mode = (argc > 3) ? argv[3] : "fast";
    bool use_best = (mode == "best");

    std::ifstream in(in_p, std::ios::binary); std::ofstream out(out_p, std::ios::binary);
    if (!in || !out) return 1;

    // 1. Header Global
    uint64_t h_sz = 0; in.read((char*)&h_sz, 8); out.write((char*)&h_sz, 8);
    std::vector<char> h_json(h_sz); in.read(h_json.data(), h_sz); out.write(h_json.data(), h_sz);

    // 2. Gravar Flag Global de Modo
    // 0 = Fast (Huffman + LSB Raw Puro), 1 = Best (Arithmetic + LSB Híbrido)
    uint8_t mode_flag = use_best ? 1 : 0;
    out.write((char*)&mode_flag, 1);

    std::cout << "Modo: " << (use_best ? "BEST (rANS + LSB Raw)" : "FAST (Huffman + LSB Raw)") << std::endl;

    std::vector<char> buf(BLOCK_SIZE);
    std::vector<uint8_t> msb, lsb;
    uint64_t tin = 0, tout = 0;
    tout += 8 + h_sz + 1; // Contabilizar header
    int blk = 0;

    while (in.read(buf.data(), BLOCK_SIZE) || in.gcount() > 0) {
        size_t n = in.gcount(), pairs = n / 2;
        msb.clear(); lsb.clear();
        for (size_t i = 0; i < pairs * 2; i += 2) {
            lsb.push_back((uint8_t)buf[i]); msb.push_back((uint8_t)buf[i+1]);
        }

        // --- Processar MSB ---
        std::vector<uint8_t> msb_enc;
        if (use_best) { AssymetricalNumericalSystem ans; ans.build(msb); msb_enc = ans.compress(msb); }
        else          { HuffmanCodec hc; hc.build(msb); msb_enc = hc.compress(msb); }

        // --- Processar LSB (CORRIGIDO) ---
        std::vector<uint8_t> lsb_final;
        
        
        lsb_final = lsb; 
        

        // --- Empacotar ---
        uint32_t sz_m = msb_enc.size(), sz_l = lsb_final.size();
        out.write((char*)&sz_m, 4); out.write((char*)&sz_l, 4);
        out.write((char*)msb_enc.data(), sz_m); out.write((char*)lsb_final.data(), sz_l);

        tin += n; tout += (8 + sz_m + sz_l);
        if (++blk % 50 == 0) std::cout << "\rBloco " << blk << " Ratio: " << std::fixed << std::setprecision(2) << (double)tin/tout << ":1" << std::flush;
    }
    std::cout << "\nFinal: " << tout / (1024.0*1024.0) << " MB. Ratio: " << std::fixed << std::setprecision(3) << (double)tin/tout << ":1" << std::endl;
    return 0;
}