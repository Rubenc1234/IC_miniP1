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

// Configurações
const size_t BLOCK_SIZE = 1024 * 1024; // 1 MB por bloco

// ==========================================
// MÓDULO AUXILIAR: HUFFMAN (Modo Fast)
// ==========================================
struct Node {
    uint8_t symbol;
    uint64_t freq;
    std::shared_ptr<Node> left, right;
    Node(uint8_t s, uint64_t f) : symbol(s), freq(f), left(nullptr), right(nullptr) {}
    Node(uint64_t f, std::shared_ptr<Node> l, std::shared_ptr<Node> r) : symbol(0), freq(f), left(l), right(r) {}
};

struct CompareNode {
    bool operator()(const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) { return a->freq > b->freq; }
};

class HuffmanCodec {
public:
    std::map<uint8_t, std::string> codes;
    std::vector<uint32_t> frequencies;

    HuffmanCodec() : frequencies(256, 0) {}

    void build(const std::vector<uint8_t>& data) {
        std::fill(frequencies.begin(), frequencies.end(), 0);
        for (uint8_t b : data) frequencies[b]++;
        std::priority_queue<std::shared_ptr<Node>, std::vector<std::shared_ptr<Node>>, CompareNode> pq;
        for (int i = 0; i < 256; ++i) if (frequencies[i] > 0) pq.push(std::make_shared<Node>((uint8_t)i, frequencies[i]));
        if (pq.empty()) return;
        if (pq.size() == 1) { codes[pq.top()->symbol] = "0"; return; }
        while (pq.size() > 1) {
            auto l = pq.top(); pq.pop(); auto r = pq.top(); pq.pop();
            pq.push(std::make_shared<Node>(l->freq + r->freq, l, r));
        }
        generate_codes(pq.top(), "");
    }

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> output;
        for (uint32_t f : frequencies) {
            output.push_back(f & 0xFF); output.push_back((f >> 8) & 0xFF);
            output.push_back((f >> 16) & 0xFF); output.push_back((f >> 24) & 0xFF);
        }
        uint8_t acc = 0; int bc = 0;
        for (uint8_t s : data) {
            for (char bit : codes[s]) {
                acc = (acc << 1) | (bit - '0'); bc++;
                if (bc == 8) { output.push_back(acc); acc = 0; bc = 0; }
            }
        }
        if (bc > 0) output.push_back(acc << (8 - bc));
        return output;
    }
private:
    void generate_codes(std::shared_ptr<Node> n, std::string c) {
        if (!n) return;
        if (!n->left && !n->right) codes[n->symbol] = c;
        generate_codes(n->left, c + "0"); generate_codes(n->right, c + "1");
    }
};

// ==========================================
// MÓDULO AUXILIAR: ARITMÉTICA (Modo Best)
// ==========================================
class ArithmeticCodec {
    const uint64_t MAX_VAL = 0xFFFFFFFF, ONE_QUARTER = 0x40000000, HALF = 0x80000000, THREE_QUARTERS = 0xC0000000;
public:
    std::vector<uint32_t> frequencies;
    std::vector<uint64_t> cumulative_freq;
    ArithmeticCodec() : frequencies(256, 0), cumulative_freq(257, 0) {}

    void build(const std::vector<uint8_t>& data) {
        std::fill(frequencies.begin(), frequencies.end(), 1); 
        for (uint8_t b : data) frequencies[b]++;
        uint64_t total = 0;
        for (int i = 0; i < 256; ++i) { cumulative_freq[i] = total; total += frequencies[i]; }
        cumulative_freq[256] = total;
    }

    std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> output;
        for (uint32_t f : frequencies) {
            output.push_back(f & 0xFF); output.push_back((f >> 8) & 0xFF);
            output.push_back((f >> 16) & 0xFF); output.push_back((f >> 24) & 0xFF);
        }
        uint64_t low = 0, high = MAX_VAL, pending = 0, total = cumulative_freq[256];
        for (uint8_t s : data) {
            uint64_t range = high - low + 1;
            high = low + (range * cumulative_freq[s + 1]) / total - 1;
            low = low + (range * cumulative_freq[s]) / total;
            while (true) {
                if (high < HALF) out_bit(0, pending, output);
                else if (low >= HALF) { out_bit(1, pending, output); low -= HALF; high -= HALF; }
                else if (low >= ONE_QUARTER && high < THREE_QUARTERS) { pending++; low -= ONE_QUARTER; high -= ONE_QUARTER; }
                else break;
                low <<= 1; high = (high << 1) | 1;
            }
        }
        pending++;
        if (low < ONE_QUARTER) out_bit(0, pending, output); else out_bit(1, pending, output);
        return output;
    }
private:
    uint8_t bb = 0; int bc = 0;
    void out_bit(int b, uint64_t& p, std::vector<uint8_t>& o) { write(b, o); while (p--) write(!b, o); p = 0; }
    void write(int b, std::vector<uint8_t>& o) {
        bb = (bb << 1) | b; bc++;
        if (bc == 8) { o.push_back(bb); bb = 0; bc = 0; }
    }
};

// ==========================================
// MÓDULO LSB: RLE (Apenas para modo Best)
// ==========================================
std::vector<uint8_t> encode_lsb_rle(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> rle_out;
    rle_out.reserve(data.size());
    size_t n = data.size();
    for (size_t i = 0; i < n; ++i) {
        if (data[i] == 0x00) {
            uint8_t count = 0;
            while (i < n && data[i] == 0x00 && count < 255) { count++; i++; }
            i--; 
            rle_out.push_back(0x00); rle_out.push_back(count);
        } else {
            rle_out.push_back(data[i]);
        }
    }
    
    // Decisão de Fallback: Se RLE compensar, usa RLE (Flag 1). Senão, Raw (Flag 0).
    std::vector<uint8_t> final_out;
    if (rle_out.size() < data.size()) {
        final_out.reserve(1 + rle_out.size());
        final_out.push_back(1); // Flag RLE
        final_out.insert(final_out.end(), rle_out.begin(), rle_out.end());
    } else {
        final_out.reserve(1 + data.size());
        final_out.push_back(0); // Flag RAW
        final_out.insert(final_out.end(), data.begin(), data.end());
    }
    return final_out;
}

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

    std::cout << "Modo: " << (use_best ? "BEST (Aritmetica + LSB RLE)" : "FAST (Huffman + LSB Raw Puro)") << std::endl;

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
        if (use_best) { ArithmeticCodec ac; ac.build(msb); msb_enc = ac.compress(msb); }
        else          { HuffmanCodec hc; hc.build(msb); msb_enc = hc.compress(msb); }

        // --- Processar LSB (CORRIGIDO) ---
        std::vector<uint8_t> lsb_final;
        
        if (use_best) {
            // Modo Best: Usa lógica RLE/Raw com flag local
            lsb_final = encode_lsb_rle(lsb);
        } else {
            // Modo Fast: Raw Puro (Sem flag local, nem tentativa de RLE)
            // Simplesmente copiamos o vetor original. Zero overhead.
            lsb_final = lsb; 
        }

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