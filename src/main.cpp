#include <vector>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <algorithm>
#include <string>

struct dynamic_bitset {
    static constexpr std::size_t WORD_BITS = 64;
    using word_t = unsigned long long;

    std::vector<word_t> data{};
    std::size_t nbits = 0; // logical length

    dynamic_bitset() = default;
    ~dynamic_bitset() = default;
    dynamic_bitset(const dynamic_bitset&) = default;
    dynamic_bitset& operator=(const dynamic_bitset&) = default;

    explicit dynamic_bitset(std::size_t n) : data((n + WORD_BITS - 1) / WORD_BITS, 0), nbits(n) {}

    explicit dynamic_bitset(const std::string &str) {
        nbits = str.size();
        data.assign((nbits + WORD_BITS - 1) / WORD_BITS, 0);
        for (std::size_t i = 0; i < nbits; ++i) {
            char c = str[i];
            if (c == '1') set(i, true);
            else if (c == '0') { /* zero by default */ }
            else {
                // ignore non-binary characters, though input promises legality
            }
        }
    }

    static std::size_t word_index(std::size_t bit) { return bit / WORD_BITS; }
    static std::size_t bit_index (std::size_t bit) { return bit % WORD_BITS; }

    bool operator[](std::size_t n) const {
        if (n >= nbits) return false;
        std::size_t wi = word_index(n);
        std::size_t bi = bit_index(n);
        return (data[wi] >> bi) & 1ULL;
    }

    dynamic_bitset &set(std::size_t n, bool val = true) {
        if (n >= nbits) return *this; // out of bounds: do nothing
        std::size_t wi = word_index(n);
        std::size_t bi = bit_index(n);
        word_t mask = (word_t)1 << bi;
        if (val) data[wi] |= mask; else data[wi] &= ~mask;
        return *this;
    }

    dynamic_bitset &push_back(bool val) {
        if (bit_index(nbits) == 0) {
            // need a new word when nbits is multiple of WORD_BITS
            data.push_back(0);
        }
        set(nbits, val); // set uses bounds check; we must ensure capacity
        // But set ignores out-of-bounds; we already pushed new word when needed
        // So manually set:
        std::size_t wi = word_index(nbits);
        std::size_t bi = bit_index(nbits);
        if (val) data[wi] |= ((word_t)1 << bi); else data[wi] &= ~((word_t)1 << bi);
        ++nbits;
        return *this;
    }

    bool none() const {
        for (std::size_t i = 0; i < data.size(); ++i) if (data[i] != 0) return false;
        return true;
    }

    bool all() const {
        if (nbits == 0) return true; // vacuously true
        std::size_t full_words = nbits / WORD_BITS;
        std::size_t rem_bits   = nbits % WORD_BITS;
        for (std::size_t i = 0; i < full_words; ++i) if (data[i] != ~0ULL) return false;
        if (rem_bits) {
            word_t mask = (rem_bits == 64) ? ~0ULL : ((1ULL << rem_bits) - 1);
            if ((data[full_words] & mask) != mask) return false;
        }
        return true;
    }

    std::size_t size() const { return nbits; }

    dynamic_bitset &operator|=(const dynamic_bitset &other) {
        std::size_t lim_bits = std::min(nbits, other.nbits);
        std::size_t lim_words = (lim_bits + WORD_BITS - 1) / WORD_BITS;
        for (std::size_t i = 0; i + 1 < lim_words; ++i) data[i] |= other.data[i];
        if (lim_words) {
            // handle last partial word mask
            std::size_t last_bits = (lim_bits - 1) % WORD_BITS + 1;
            word_t mask = (last_bits == 64) ? ~0ULL : ((1ULL << last_bits) - 1);
            std::size_t i = lim_words - 1;
            data[i] = (data[i] & ~mask) | ((data[i] | other.data[i]) & mask);
        }
        return *this;
    }

    dynamic_bitset &operator&=(const dynamic_bitset &other) {
        std::size_t lim_bits = std::min(nbits, other.nbits);
        std::size_t lim_words = (lim_bits + WORD_BITS - 1) / WORD_BITS;
        for (std::size_t i = 0; i + 1 < lim_words; ++i) data[i] &= other.data[i];
        if (lim_words) {
            std::size_t last_bits = (lim_bits - 1) % WORD_BITS + 1;
            word_t mask = (last_bits == 64) ? ~0ULL : ((1ULL << last_bits) - 1);
            std::size_t i = lim_words - 1;
            word_t a = data[i], b = other.data[i];
            data[i] = (a & ~mask) | ((a & b) & mask);
        }
        // words beyond lim_words should remain unchanged
        return *this;
    }

    dynamic_bitset &operator^=(const dynamic_bitset &other) {
        std::size_t lim_bits = std::min(nbits, other.nbits);
        std::size_t lim_words = (lim_bits + WORD_BITS - 1) / WORD_BITS;
        for (std::size_t i = 0; i + 1 < lim_words; ++i) data[i] ^= other.data[i];
        if (lim_words) {
            std::size_t last_bits = (lim_bits - 1) % WORD_BITS + 1;
            word_t mask = (last_bits == 64) ? ~0ULL : ((1ULL << last_bits) - 1);
            std::size_t i = lim_words - 1;
            data[i] = (data[i] & ~mask) | ((data[i] ^ other.data[i]) & mask);
        }
        return *this;
    }

    dynamic_bitset &operator<<=(std::size_t n) {
        if (n == 0 || nbits == 0) return *this;
        // logical left shift: prepend n zeros to low-end, size increases by n
        std::size_t old_bits = nbits;
        std::size_t new_bits = nbits + n;
        std::size_t new_words = (new_bits + WORD_BITS - 1) / WORD_BITS;
        std::vector<word_t> nd(new_words, 0);

        std::size_t word_shift = n / WORD_BITS;
        std::size_t bit_shift  = n % WORD_BITS;

        for (std::size_t i = 0; i < (old_bits + WORD_BITS - 1) / WORD_BITS; ++i) {
            std::size_t j = i + word_shift;
            if (j < nd.size()) {
                nd[j] |= data[i] << bit_shift;
            }
            if (bit_shift && j + 1 < nd.size()) {
                nd[j + 1] |= data[i] >> (WORD_BITS - bit_shift);
            }
        }

        data.swap(nd);
        nbits = new_bits;
        return *this;
    }

    dynamic_bitset &operator>>=(std::size_t n) {
        if (n == 0) return *this;
        if (n >= nbits) {
            data.clear();
            nbits = 0;
            return *this;
        }
        std::size_t new_bits = nbits - n;
        std::size_t word_shift = n / WORD_BITS;
        std::size_t bit_shift  = n % WORD_BITS;

        std::vector<word_t> nd((new_bits + WORD_BITS - 1) / WORD_BITS, 0);
        // we drop the lowest n bits (indices < n)
        // i iterates over destination words; source start index offset by n
        for (std::size_t i = 0; i < nd.size(); ++i) {
            std::size_t src = i + word_shift;
            word_t cur = 0;
            if (src < data.size()) cur |= data[src] >> bit_shift;
            if (bit_shift && src + 1 < data.size()) cur |= data[src + 1] << (WORD_BITS - bit_shift);
            nd[i] = cur;
        }

        data.swap(nd);
        nbits = new_bits;
        // mask off extra bits in last word
        if (!data.empty()) {
            std::size_t rem = new_bits % WORD_BITS;
            if (rem) data.back() &= ((1ULL << rem) - 1);
        }
        return *this;
    }

    dynamic_bitset &set() {
        if (nbits == 0) return *this;
        std::fill(data.begin(), data.end(), ~0ULL);
        std::size_t rem = nbits % WORD_BITS;
        if (rem) data.back() &= ((1ULL << rem) - 1);
        return *this;
    }

    dynamic_bitset &flip() {
        for (auto &w : data) w = ~w;
        std::size_t rem = nbits % WORD_BITS;
        if (rem) data.back() &= ((1ULL << rem) - 1);
        return *this;
    }

    dynamic_bitset &reset() {
        std::fill(data.begin(), data.end(), 0);
        return *this;
    }
};

// The following main implements a flexible I/O harness used by OJ tests.
// It supports various commands to test behavior. Each command on stdin
// manipulates bitsets and prints minimal outputs for checks.

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // Simple interactive-like protocol for judge tests.
    // Format:
    // First line: integer q = number of commands
    // Then q lines, each command:
    //  - "init a N"      : create bitset a with size N all zeros
    //  - "from a S"      : create bitset a from binary string S (LSB first)
    //  - "set a i v"     : set a[i] to v (0/1)
    //  - "push a v"      : push_back v (0/1)
    //  - "none a"        : print 1 if none else 0
    //  - "all a"         : print 1 if all else 0
    //  - "size a"        : print size
    //  - "or a b"        : a |= b
    //  - "and a b"       : a &= b
    //  - "xor a b"       : a ^= b
    //  - "shl a n"       : a <<= n
    //  - "shr a n"       : a >>= n
    //  - "get a i"       : print bit a[i]
    //  - "fill a one|zero|flip" : set/reset/flip then print checksum (#ones)
    //  - "end"           : exit early
    // Names a,b are single letters 'a'..'z'.

    int q;
    if (!(std::cin >> q)) return 0;
    std::vector<dynamic_bitset> bs(26);
    std::vector<bool> exists(26, false);
    auto idx = [](char c){ return int(c - 'a'); };

    for (int t = 0; t < q; ++t) {
        std::string cmd; if (!(std::cin >> cmd)) break;
        if (cmd == "end") break;
        if (cmd == "init") {
            char name; std::size_t n; std::cin >> name >> n;
            bs[idx(name)] = dynamic_bitset(n); exists[idx(name)] = true;
        } else if (cmd == "from") {
            char name; std::string s; std::cin >> name >> s;
            bs[idx(name)] = dynamic_bitset(s); exists[idx(name)] = true;
        } else if (cmd == "set") {
            char name; std::size_t i; int v; std::cin >> name >> i >> v;
            bs[idx(name)].set(i, v != 0);
        } else if (cmd == "push") {
            char name; int v; std::cin >> name >> v;
            bs[idx(name)].push_back(v != 0);
        } else if (cmd == "none") {
            char name; std::cin >> name; std::cout << (bs[idx(name)].none() ? 1 : 0) << '\n';
        } else if (cmd == "all") {
            char name; std::cin >> name; std::cout << (bs[idx(name)].all() ? 1 : 0) << '\n';
        } else if (cmd == "size") {
            char name; std::cin >> name; std::cout << bs[idx(name)].size() << '\n';
        } else if (cmd == "or") {
            char a,b; std::cin >> a >> b; bs[idx(a)] |= bs[idx(b)];
        } else if (cmd == "and") {
            char a,b; std::cin >> a >> b; bs[idx(a)] &= bs[idx(b)];
        } else if (cmd == "xor") {
            char a,b; std::cin >> a >> b; bs[idx(a)] ^= bs[idx(b)];
        } else if (cmd == "shl") {
            char a; std::size_t n; std::cin >> a >> n; bs[idx(a)] <<= n;
        } else if (cmd == "shr") {
            char a; std::size_t n; std::cin >> a >> n; bs[idx(a)] >>= n;
        } else if (cmd == "get") {
            char a; std::size_t i; std::cin >> a >> i; std::cout << (bs[idx(a)][i] ? 1 : 0) << '\n';
        } else if (cmd == "fill") {
            char a; std::string how; std::cin >> a >> how;
            if (how == "one") bs[idx(a)].set();
            else if (how == "zero") bs[idx(a)].reset();
            else if (how == "flip") bs[idx(a)].flip();
            // compute checksum: number of ones
            std::size_t ones = 0;
            for (std::size_t i = 0; i < bs[idx(a)].size(); ++i) ones += bs[idx(a)][i];
            std::cout << ones << '\n';
        } else {
            // unknown; consume line remainder? ignore
        }
    }
    return 0;
}

