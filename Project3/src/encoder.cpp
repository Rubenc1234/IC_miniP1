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
    static constexpr uint32_t TOT = 1u << 12;
    static constexpr uint32_t SHIFT = 12;
    static constexpr uint64_t LOWER = 1ULL << 32;
    static constexpr uint64_t UPPER = 1ULL << 40;

    std::vector<uint32_t> freq;
    std::vector<uint32_t> norm_freq;
    std::vector<uint32_t> cumul;

    void build(const std::vector<uint8_t>& data) {
        freq.assign(256, 0);
        for (uint8_t b : data) freq[b]++;
        normalize_freq();
        build_cumul();
    }

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> out;

        // 1) Serializar freq original (256 * 4 bytes little-endian)
        for (int i = 0; i < 256; ++i) {
            uint32_t f = freq[i];
            out.push_back(static_cast<uint8_t>(f & 0xFF));
            out.push_back(static_cast<uint8_t>((f >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>((f >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((f >> 24) & 0xFF));
        }

        // 2) rANS encode
        std::vector<uint8_t> payload;
        payload.reserve(data.size());

        uint64_t state = LOWER; // estado inicial

        // Encode em ordem reversa
        for (size_t i = data.size(); i-- > 0; ) {
            uint8_t s = data[i];
            uint32_t f = norm_freq[s];
            uint32_t start = cumul[s];

            // Renormalizar ANTES de codificar: se state >= f * UPPER / TOT, emite bytes
            uint64_t threshold = ((UPPER >> SHIFT) * f);
            while (state >= threshold) {
                payload.push_back(state & 0xFF);
                state >>= 8;
            }

            // Codificar símbolo
            // state' = (state / f) * TOT + (state % f) + start
            state = ((state / f) << SHIFT) + (state % f) + start;
        }

        // Flush estado final (4-8 bytes)
        while (state > 0) {
            payload.push_back(state & 0xFF);
            state >>= 8;
        }

        // Reverter payload (porque escrevemos ao contrário)
        std::reverse(payload.begin(), payload.end());

        out.insert(out.end(), payload.begin(), payload.end());
        return out;
    }

private:
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

        // Ajustar para somar exatamente TOT
        if (sum < TOT) {
            std::vector<int> idx(256);
            for (int i = 0; i < 256; ++i) idx[i] = i;
            std::sort(idx.begin(), idx.end(), [&](int a, int b) { return freq[a] > freq[b]; });
            size_t p = 0;
            while (sum < TOT) {
                int s = idx[p % 256];
                if (freq[s] > 0) { norm_freq[s]++; sum++; }
                p++;
            }
        } else if (sum > TOT) {
            std::vector<int> idx(256);
            for (int i = 0; i < 256; ++i) idx[i] = i;
            std::sort(idx.begin(), idx.end(), [&](int a, int b) { return freq[a] < freq[b]; });
            size_t p = 0;
            while (sum > TOT) {
                int s = idx[p % 256];
                if (norm_freq[s] > 1) { norm_freq[s]--; sum--; }
                p++;
            }
        }
    }

    void build_cumul() {
        cumul.assign(257, 0);
        for (int i = 0; i < 256; ++i) cumul[i + 1] = cumul[i] + norm_freq[i];
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