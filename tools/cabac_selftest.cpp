#include "../src/h264i.cpp"
#include <vector>
#include <random>
struct Enc {
  uint32_t low = 0, range = 510; int outstanding = 0; bool first = true; std::vector<int> bits; uint8_t st[1024];
  void putbit(int b) { if (first) first = false; else bits.push_back(b); while (outstanding > 0) { bits.push_back(1 - b); outstanding--; } }
  void renorm() { while (range < 256) { if (low < 256) putbit(0); else if (low >= 512) { low -= 512; putbit(1); } else { low -= 256; outstanding++; } range <<= 1; low <<= 1; } }
  void encode(int ctx, int bin) {
    int p = st[ctx] >> 1, mps = st[ctx] & 1; uint32_t rl = H264I_LPS[p][(range >> 6) & 3]; range -= rl;
    if (bin != mps) { low += range; range = rl; if (p == 0) mps = 1 - mps; p = TLPS[p]; } else { if (p < 62) p++; }
    st[ctx] = (p << 1) | mps; renorm();
  }
  void bypass(int bin) { low <<= 1; if (bin) low += range; if (low >= 1024) { putbit(1); low -= 1024; } else if (low < 512) putbit(0); else { low -= 512; outstanding++; } }
  void flush() { range = 2; renorm(); putbit((low >> 9) & 1); int v = ((low >> 7) & 3) | 1; bits.push_back(v >> 1); bits.push_back(v & 1); }
};
int main() {
  std::mt19937 rng(1);
  for (int trial = 0; trial < 50; trial++) {
    Cabac dc; Enc en; int qp = rng() % 52; dc.init_ctx(qp); memcpy(en.st, dc.st, 1024);
    std::vector<int> ctxs, vals; int N = 5000;
    for (int i = 0; i < N; i++) { int c = rng() % 1024; int mode = rng() % 10; int v; if (mode == 0) v = -1; else v = (rng() % 100) < 70 ? 1 : 0;
      ctxs.push_back(mode == 0 ? -1 : c); vals.push_back(mode == 0 ? (int)(rng() & 1) : v); }
    for (int i = 0; i < N; i++) { if (ctxs[i] < 0) en.bypass(vals[i]); else en.encode(ctxs[i], vals[i]); }
    en.flush();
    std::vector<uint8_t> bytes((en.bits.size() + 7) / 8 + 4, 0);
    for (size_t i = 0; i < en.bits.size(); i++) if (en.bits[i]) bytes[i >> 3] |= 0x80 >> (i & 7);
    dc.start(bytes.data(), bytes.size(), 0);
    for (int i = 0; i < N; i++) { int v = ctxs[i] < 0 ? dc.bypass() : dc.dec(ctxs[i]); if (v != vals[i]) { printf("trial %d mismatch at %d (ctx %d)\n", trial, i, ctxs[i]); return 1; } }
  }
  printf("moteur CABAC OK\n"); return 0;
}
