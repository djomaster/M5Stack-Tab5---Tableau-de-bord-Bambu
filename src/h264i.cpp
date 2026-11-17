// Decodeur H.264 intra seulement (voir h264i.h).
#include "h264i.h"
#include <stdlib.h>
#include <string.h>
#include "h264i_tables.h"

struct H264IMb {
  int16_t slice;
  uint8_t type;  // 0 : I_NxN, 1 : I_16x16
  uint8_t t8, cbp_l, cbp_c, chroma_mode, dcflags, cbf_cb, cbf_cr;
  uint16_t cbf_l;
  int8_t pm[16];  // modes de prediction 4x4 (grille 4x4 de l'macrobloc, x + 4*y)
};

namespace {

const uint8_t TLPS[64] = {0, 0, 1, 2, 2, 4, 4, 5, 6, 7, 8, 9, 9, 11, 11, 12, 13, 13, 15, 15, 16, 16, 18, 18, 19, 19, 21,
                          21, 22, 22, 23, 24, 24, 25, 26, 26, 27, 27, 28, 29, 29, 30, 30, 30, 31, 32, 32, 33, 33, 33, 34,
                          34, 35, 35, 35, 36, 36, 36, 37, 37, 37, 38, 38, 63};
const uint8_t SIG8[63] = {0, 1, 2, 3, 4, 5, 5, 4, 4, 3, 3, 4, 4, 4, 5, 5, 4, 4, 4, 4, 3, 3, 6, 7, 7, 7, 8, 9, 10, 9, 8, 7,
                          7, 6, 11, 12, 13, 11, 6, 7, 8, 9, 14, 10, 9, 8, 6, 11, 12, 13, 11, 6, 9, 14, 10, 9, 11, 12, 13, 11, 14, 10, 12};
const uint8_t LAST8[63] = {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
                           3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8};
const uint8_t NORM4[6][3] = {{10, 16, 13}, {11, 18, 14}, {13, 20, 16}, {14, 23, 18}, {16, 25, 20}, {18, 29, 23}};
const uint8_t NORM8[6][6] = {{20, 18, 32, 19, 25, 24}, {22, 19, 35, 21, 28, 26}, {26, 23, 42, 24, 33, 31},
                             {28, 25, 45, 26, 35, 33}, {32, 28, 51, 30, 40, 38}, {36, 32, 58, 34, 46, 43}};
const uint8_t QPC[22] = {29, 30, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 39};
// blocs 4x4 : numero (ordre de decodage) -> position dans la grille
const uint8_t BLK_X[16] = {0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3};
const uint8_t BLK_Y[16] = {0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3};

uint8_t ZZ4[16], ZZ8[64];
bool zz_ready = false;
void make_zigzag(uint8_t *out, int n) {
  int k = 0;
  for (int s = 0; s <= 2 * (n - 1); s++) {
    if (s & 1) {
      for (int y = 0; y <= s; y++) {
        int x = s - y;
        if (x < n && y < n) out[k++] = (uint8_t)(y * n + x);
      }
    } else {
      for (int y = s; y >= 0; y--) {
        int x = s - y;
        if (x < n && y < n) out[k++] = (uint8_t)(y * n + x);
      }
    }
  }
}

inline int clip255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
inline int clip3(int lo, int hi, int v) { return v < lo ? lo : (v > hi ? hi : v); }

struct Bits {
  const uint8_t *d;
  size_t n;
  size_t pos = 0;
  uint32_t u1() {
    if (pos >= n * 8) {
      pos++;
      return 0;
    }
    uint32_t v = (d[pos >> 3] >> (7 - (pos & 7))) & 1;
    pos++;
    return v;
  }
  uint32_t u(int k) {
    uint32_t v = 0;
    while (k-- > 0) v = (v << 1) | u1();
    return v;
  }
  uint32_t ue() {
    int z = 0;
    while (z < 32 && pos < n * 8 && u1() == 0) z++;
    if (z == 0) return 0;
    return ((1u << z) - 1) + u(z);
  }
  int32_t se() {
    uint32_t k = ue();
    return (k & 1) ? (int32_t)((k + 1) / 2) : -(int32_t)(k / 2);
  }
};

void skip_scaling(Bits &b, int size) {
  int last = 8, next = 8;
  for (int j = 0; j < size; j++) {
    if (next != 0) next = (last + b.se() + 256) % 256;
    last = (next == 0) ? last : next;
  }
}

size_t unescape(const uint8_t *in, size_t n, uint8_t *out) {
  size_t o = 0;
  int zeros = 0;
  for (size_t i = 0; i < n; i++) {
    if (zeros >= 2 && in[i] == 3) {
      zeros = 0;
      continue;
    }
    zeros = (in[i] == 0) ? zeros + 1 : 0;
    out[o++] = in[i];
  }
  return o;
}

// ---------------------------------------------------------------- CABAC
struct Cabac {
  const uint8_t *d = nullptr;
  size_t nbits = 0, pos = 0;
  uint32_t range = 510, offset = 0;
  uint8_t st[1024];

  inline uint32_t bit() {
    if (pos >= nbits) {
      pos++;
      return 0;
    }
    uint32_t v = (d[pos >> 3] >> (7 - (pos & 7))) & 1;
    pos++;
    return v;
  }
  void start(const uint8_t *data, size_t bytes, size_t byte_pos) {
    d = data;
    nbits = bytes * 8;
    pos = byte_pos * 8;
    range = 510;
    offset = 0;
    for (int i = 0; i < 9; i++) offset = (offset << 1) | bit();
  }
  void init_ctx(int qp) {
    int q = clip3(0, 51, qp);
    for (int i = 0; i < 1024; i++) {
      int pre = clip3(1, 126, ((H264I_CABAC_INIT_I[i][0] * q) >> 4) + H264I_CABAC_INIT_I[i][1]);
      st[i] = (pre <= 63) ? (uint8_t)((63 - pre) << 1) : (uint8_t)(((pre - 64) << 1) | 1);
    }
  }
  inline int dec(int ctx) {
    uint8_t s = st[ctx];
    int p = s >> 1, mps = s & 1;
    uint32_t rl = H264I_LPS[p][(range >> 6) & 3];
    range -= rl;
    int b;
    if (offset >= range) {
      b = !mps;
      offset -= range;
      range = rl;
      if (p == 0) mps = !mps;
      p = TLPS[p];
    } else {
      b = mps;
      if (p < 62) p++;
    }
    st[ctx] = (uint8_t)((p << 1) | mps);
    while (range < 256) {
      range <<= 1;
      offset = (offset << 1) | bit();
    }
    return b;
  }
  inline int bypass() {
    offset = (offset << 1) | bit();
    if (offset >= range) {
      offset -= range;
      return 1;
    }
    return 0;
  }
  inline int term() {
    range -= 2;
    if (offset >= range) return 1;
    while (range < 256) {
      range <<= 1;
      offset = (offset << 1) | bit();
    }
    return 0;
  }
};

// ---------------------------------------------------------------- transformees
void idct4_add(uint8_t *dst, int stride, const int32_t *c) {
  int32_t t[16];
  for (int i = 0; i < 4; i++) {
    const int32_t *r = c + i * 4;
    int a = r[0] + r[2], b = r[0] - r[2], cc = (r[1] >> 1) - r[3], d = r[1] + (r[3] >> 1);
    t[i * 4 + 0] = a + d;
    t[i * 4 + 1] = b + cc;
    t[i * 4 + 2] = b - cc;
    t[i * 4 + 3] = a - d;
  }
  for (int j = 0; j < 4; j++) {
    int a = t[j] + t[8 + j], b = t[j] - t[8 + j], cc = (t[4 + j] >> 1) - t[12 + j], d = t[4 + j] + (t[12 + j] >> 1);
    int v[4] = {a + d, b + cc, b - cc, a - d};
    for (int i = 0; i < 4; i++) dst[i * stride + j] = (uint8_t)clip255(dst[i * stride + j] + ((v[i] + 32) >> 6));
  }
}

void idct8_1d(const int32_t *in, int step, int32_t *out) {
  int d0 = in[0], d1 = in[step], d2 = in[2 * step], d3 = in[3 * step], d4 = in[4 * step], d5 = in[5 * step], d6 = in[6 * step],
      d7 = in[7 * step];
  int a0 = d0 + d4, a1 = -d3 + d5 - d7 - (d7 >> 1), a2 = d0 - d4, a3 = d1 + d7 - d3 - (d3 >> 1);
  int a4 = (d2 >> 1) - d6, a5 = -d1 + d7 + d5 + (d5 >> 1), a6 = d2 + (d6 >> 1), a7 = d3 + d5 + d1 + (d1 >> 1);
  int b0 = a0 + a6, b1 = a1 + (a7 >> 2), b2 = a2 + a4, b3 = a3 + (a5 >> 2), b4 = a2 - a4, b5 = (a3 >> 2) - a5, b6 = a0 - a6,
      b7 = a7 - (a1 >> 2);
  out[0] = b0 + b7;
  out[step] = b2 + b5;
  out[2 * step] = b4 + b3;
  out[3 * step] = b6 + b1;
  out[4 * step] = b6 - b1;
  out[5 * step] = b4 - b3;
  out[6 * step] = b2 - b5;
  out[7 * step] = b0 - b7;
}

void idct8_add(uint8_t *dst, int stride, const int32_t *c) {
  int32_t t[64], u[64];
  for (int i = 0; i < 8; i++) idct8_1d(c + i * 8, 1, t + i * 8);
  for (int j = 0; j < 8; j++) idct8_1d(t + j, 8, u + j);
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 8; j++) dst[i * stride + j] = (uint8_t)clip255(dst[i * stride + j] + ((u[i * 8 + j] + 32) >> 6));
}

// ---------------------------------------------------------------- prediction intra
// top[0] = coin haut gauche, top[1..] = p[x,-1] ; left[0] = coin, left[1..] = p[-1,y]
void pred_nxn(uint8_t *dst, int stride, int n, int mode, const int *top, const int *left, bool hasL, bool hasT) {
  auto T = [&](int x) { return top[x + 1]; };   // p[x,-1], x >= -1
  auto L = [&](int y) { return left[y + 1]; };  // p[-1,y], y >= -1
  int sh = (n == 4) ? 2 : 3;
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++) {
      int v = 128;
      switch (mode) {
        case 0: v = T(x); break;
        case 1: v = L(y); break;
        case 2: {
          int s = 0;
          if (hasL && hasT) {
            for (int i = 0; i < n; i++) s += T(i) + L(i);
            v = (s + n) >> (sh + 1);
          } else if (hasL) {
            for (int i = 0; i < n; i++) s += L(i);
            v = (s + n / 2) >> sh;
          } else if (hasT) {
            for (int i = 0; i < n; i++) s += T(i);
            v = (s + n / 2) >> sh;
          }
          break;
        }
        case 3:
          v = (x == n - 1 && y == n - 1) ? (T(2 * n - 2) + 3 * T(2 * n - 1) + 2) >> 2
                                         : (T(x + y) + 2 * T(x + y + 1) + T(x + y + 2) + 2) >> 2;
          break;
        case 4:
          if (x > y) v = (T(x - y - 2) + 2 * T(x - y - 1) + T(x - y) + 2) >> 2;
          else if (x < y) v = (L(y - x - 2) + 2 * L(y - x - 1) + L(y - x) + 2) >> 2;
          else v = (T(0) + 2 * T(-1) + L(0) + 2) >> 2;
          break;
        case 5: {
          int z = 2 * x - y;
          if (z >= 0 && !(z & 1)) v = (T(x - (y >> 1) - 1) + T(x - (y >> 1)) + 1) >> 1;
          else if (z > 0) v = (T(x - (y >> 1) - 2) + 2 * T(x - (y >> 1) - 1) + T(x - (y >> 1)) + 2) >> 2;
          else if (z == -1) v = (L(0) + 2 * L(-1) + T(0) + 2) >> 2;
          else v = (L(y - 2 * x - 1) + 2 * L(y - 2 * x - 2) + L(y - 2 * x - 3) + 2) >> 2;
          break;
        }
        case 6: {
          int z = 2 * y - x;
          if (z >= 0 && !(z & 1)) v = (L(y - (x >> 1) - 1) + L(y - (x >> 1)) + 1) >> 1;
          else if (z > 0) v = (L(y - (x >> 1) - 2) + 2 * L(y - (x >> 1) - 1) + L(y - (x >> 1)) + 2) >> 2;
          else if (z == -1) v = (L(0) + 2 * L(-1) + T(0) + 2) >> 2;
          else v = (T(x - 2 * y - 1) + 2 * T(x - 2 * y - 2) + T(x - 2 * y - 3) + 2) >> 2;
          break;
        }
        case 7:
          if (!(y & 1)) v = (T(x + (y >> 1)) + T(x + (y >> 1) + 1) + 1) >> 1;
          else v = (T(x + (y >> 1)) + 2 * T(x + (y >> 1) + 1) + T(x + (y >> 1) + 2) + 2) >> 2;
          break;
        case 8: {
          int z = x + 2 * y, lim = 2 * n - 3;
          if (z > lim) v = L(n - 1);
          else if (z == lim) v = (L(n - 2) + 3 * L(n - 1) + 2) >> 2;
          else if (!(z & 1)) v = (L(y + (x >> 1)) + L(y + (x >> 1) + 1) + 1) >> 1;
          else v = (L(y + (x >> 1)) + 2 * L(y + (x >> 1) + 1) + L(y + (x >> 1) + 2) + 2) >> 2;
          break;
        }
      }
      dst[y * stride + x] = (uint8_t)v;
    }
}

}  // namespace

// =====================================================================
//  Etat de decodage d'une tranche
// =====================================================================
struct H264ISlice {
  H264Intra &D;
  Cabac cb;
  int qp = 26, last_dqp = 0, sid = 0;
  int qpc[2] = {0, 0};
  bool t8mode = false;
  int cqp_off[2];

  // coefficients du macrobloc courant (deja dequantifies)
  int32_t c4[16][16];
  uint8_t nz4[16];
  int32_t c8[4][64];
  uint8_t nz8[4];
  int32_t dcl[16];
  int32_t cdc[2][4];
  int32_t cac[2][4][16];
  uint8_t cnz[2][4];
  bool has_dc_l;

  explicit H264ISlice(H264Intra &d) : D(d) {}

  H264IMb &mbat(int a) { return D.mb_[a]; }

  // ---- bloc residuel CABAC ; renvoie le nombre de coefficients non nuls, 'lev' (taille maxn) recoit les niveaux en ordre de scan
  int residual(int cat, int cbf_ctx, int maxn, int *lev) {
    static const int sig_off[5] = {0, 15, 29, 44, 47}, abs_off[5] = {0, 10, 20, 30, 39};
    for (int i = 0; i < maxn; i++) lev[i] = 0;
    if (cbf_ctx >= 0 && !cb.dec(cbf_ctx)) return 0;
    int sigb, lastb, absb, so;
    if (cat == 5) {
      sigb = 402;
      lastb = 417;
      absb = 426;
      so = 0;
    } else {
      sigb = 105 + sig_off[cat];
      lastb = 166 + sig_off[cat];
      absb = 227 + abs_off[cat];
      so = 0;
    }
    (void)so;
    int pos[64], np = 0;
    int i = 0;
    for (; i < maxn - 1; i++) {
      int si, li;
      if (cat == 5) {
        si = sigb + SIG8[i];
        li = lastb + LAST8[i];
      } else if (cat == 3) {
        si = sigb + (i < 2 ? i : 2);
        li = lastb + (i < 2 ? i : 2);
      } else {
        si = sigb + i;
        li = lastb + i;
      }
      if (cb.dec(si)) {
        pos[np++] = i;
        if (cb.dec(li)) break;
      }
    }
    if (i == maxn - 1) pos[np++] = maxn - 1;  // dernier coefficient deduit significatif
    int eq1 = 0, gt1 = 0;
    for (int k = np - 1; k >= 0; k--) {
      int ctx0 = absb + ((gt1 != 0) ? 0 : (1 + eq1 < 4 ? 1 + eq1 : 4));
      int a;
      if (!cb.dec(ctx0)) {
        a = 0;
      } else {
        int lim = 4 - (cat == 3 ? 1 : 0);
        int ctxn = absb + 5 + (gt1 < lim ? gt1 : lim);
        a = 1;
        while (a < 14 && cb.dec(ctxn)) a++;
        if (a == 14) {
          int e = 0;
          while (cb.bypass() && e < 24) {
            a += 1 << e;
            e++;
          }
          while (e-- > 0) a += cb.bypass() << e;
        }
      }
      a += 1;
      if (a == 1) eq1++;
      else gt1++;
      lev[pos[k]] = cb.bypass() ? -a : a;
    }
    return np;
  }

  // ---- decodage d'un macrobloc
  bool decode_mb(int addr) {
    const int mbw = D.mbw_;
    int mx = addr % mbw, my = addr / mbw;
    H264IMb &m = mbat(addr);
    int aL_ = (mx > 0) ? addr - 1 : -1;
    int aT_ = (my > 0) ? addr - mbw : -1;
    int aTR_ = (my > 0 && mx + 1 < mbw) ? addr - mbw + 1 : -1;
    int aTL_ = (my > 0 && mx > 0) ? addr - mbw - 1 : -1;
    bool aL = aL_ >= 0 && mbat(aL_).slice == sid, aT = aT_ >= 0 && mbat(aT_).slice == sid;
    bool aTR = aTR_ >= 0 && mbat(aTR_).slice == sid, aTL = aTL_ >= 0 && mbat(aTL_).slice == sid;
    H264IMb *mL = aL ? &mbat(aL_) : nullptr, *mT = aT ? &mbat(aT_) : nullptr;

    memset(&m, 0, sizeof(m));
    m.slice = (int16_t)sid;
    memset(c4, 0, sizeof(c4));
    memset(nz4, 0, sizeof(nz4));
    memset(c8, 0, sizeof(c8));
    memset(nz8, 0, sizeof(nz8));
    memset(dcl, 0, sizeof(dcl));
    memset(cdc, 0, sizeof(cdc));
    memset(cac, 0, sizeof(cac));
    memset(cnz, 0, sizeof(cnz));
    has_dc_l = false;

    // type de macrobloc
    int inc = (mL && mL->type != 0) + (mT && mT->type != 0);
    int i16mode = 0, i16luma = 0, i16chroma = 0;
    if (!cb.dec(3 + inc)) {
      m.type = 0;
    } else {
      if (cb.term()) return D.fail("I_PCM non pris en charge");
      m.type = 1;
      i16luma = cb.dec(6);
      if (cb.dec(7)) i16chroma = cb.dec(8) ? 2 : 1;
      int hi = cb.dec(9), lo = cb.dec(10);
      i16mode = hi * 2 + lo;
    }
    if (m.type == 1) {
      m.cbp_l = i16luma ? 15 : 0;
      m.cbp_c = (uint8_t)i16chroma;
      for (int i = 0; i < 16; i++) m.pm[i] = 2;
    }
    if (m.type == 0) {
      if (t8mode) m.t8 = (uint8_t)cb.dec(399 + (mL && mL->t8) + (mT && mT->t8));
      int nb = m.t8 ? 4 : 16;
      for (int k = 0; k < nb; k++) {
        int gx, gy;
        if (m.t8) {
          gx = (k & 1) * 2;
          gy = (k >> 1) * 2;
        } else {
          gx = BLK_X[k];
          gy = BLK_Y[k];
        }
        int ma, mb2;
        bool ua = false, ub = false;
        if (gx > 0) {
          ma = m.pm[gy * 4 + gx - 1];
          ua = true;
        } else if (mL) {
          ma = mL->type == 0 ? mL->pm[gy * 4 + 3] : 2;
          ua = true;
        } else ma = 2;
        if (gy > 0) {
          mb2 = m.pm[(gy - 1) * 4 + gx];
          ub = true;
        } else if (mT) {
          mb2 = mT->type == 0 ? mT->pm[12 + gx] : 2;
          ub = true;
        } else mb2 = 2;
        int pred = (ua && ub) ? (ma < mb2 ? ma : mb2) : 2;
        int mode;
        if (cb.dec(68)) mode = pred;
        else {
          int rem = cb.dec(69);
          rem |= cb.dec(69) << 1;
          rem |= cb.dec(69) << 2;
          mode = rem < pred ? rem : rem + 1;
        }
        if (m.t8) {
          m.pm[gy * 4 + gx] = m.pm[gy * 4 + gx + 1] = m.pm[(gy + 1) * 4 + gx] = m.pm[(gy + 1) * 4 + gx + 1] = (int8_t)mode;
        } else m.pm[gy * 4 + gx] = (int8_t)mode;
      }
    }
    // mode de prediction chroma
    {
      int ci = (mL && mL->chroma_mode != 0) + (mT && mT->chroma_mode != 0);
      int cm = 0;
      if (cb.dec(64 + ci)) {
        cm = 1;
        if (cb.dec(67)) cm = cb.dec(67) ? 3 : 2;
      }
      m.chroma_mode = (uint8_t)cm;
    }
    // cbp (hors I16x16)
    if (m.type == 0) {
      int cbp = 0;
      for (int b8 = 0; b8 < 4; b8++) {
        int ca, cbb;
        if (b8 & 1) ca = ((cbp >> (b8 - 1)) & 1) == 0;
        else ca = mL ? (((mL->cbp_l >> (b8 + 1)) & 1) == 0) : 0;
        if (b8 >= 2) cbb = ((cbp >> (b8 - 2)) & 1) == 0;
        else cbb = mT ? (((mT->cbp_l >> (b8 + 2)) & 1) == 0) : 0;
        cbp |= cb.dec(73 + ca + 2 * cbb) << b8;
      }
      int cc = 0;
      int ca = mL && mL->cbp_c != 0, cbb = mT && mT->cbp_c != 0;
      if (cb.dec(77 + ca + 2 * cbb)) {
        ca = mL && mL->cbp_c == 2;
        cbb = mT && mT->cbp_c == 2;
        cc = cb.dec(77 + 4 + ca + 2 * cbb) ? 2 : 1;
      }
      m.cbp_l = (uint8_t)cbp;
      m.cbp_c = (uint8_t)cc;
    }
    // ecart de quantification
    if (m.type == 1 || m.cbp_l || m.cbp_c) {
      int d = 0;
      if (cb.dec(60 + (last_dqp != 0))) {
        int k = 1;
        if (cb.dec(62)) {
          k = 2;
          while (cb.dec(63) && k < 200) k++;
        }
        d = (k & 1) ? (k + 1) / 2 : -(k / 2);
      }
      last_dqp = d;
      qp = ((qp + d) % 52 + 52) % 52;
    } else {
      last_dqp = 0;
    }
    for (int c = 0; c < 2; c++) {
      int qpi = clip3(0, 51, qp + cqp_off[c]);
      qpc[c] = qpi < 30 ? qpi : QPC[qpi - 30];
    }

    // ---- residus
    if (!residual_luma(m, mL, mT, aL, aT, i16mode)) return false;
    residual_chroma(m, mL, mT, aL, aT);

    // ---- reconstruction
    reconstruct(m, mx, my, aL, aT, aTR, aTL, i16mode);
    return true;
  }

  // indice de contexte du drapeau de bloc code
  int cbf_inc_luma(H264IMb &m, H264IMb *mL, H264IMb *mT, bool aL, bool aT, int gx, int gy) {
    int a, b;
    if (gx > 0) a = (m.cbf_l >> (gy * 4 + gx - 1)) & 1;
    else a = aL ? ((mL->cbf_l >> (gy * 4 + 3)) & 1) : 1;
    if (gy > 0) b = (m.cbf_l >> ((gy - 1) * 4 + gx)) & 1;
    else b = aT ? ((mT->cbf_l >> (12 + gx)) & 1) : 1;
    return a + 2 * b;
  }

  void dequant4(int32_t *out, const int *lev, int first, int n, int q) {
    int sh = q / 6;
    const uint8_t *nm = NORM4[q % 6];
    for (int k = 0; k < n; k++) {
      if (!lev[k]) continue;
      int r = ZZ4[k + first];
      int i = r >> 2, j = r & 3;
      int v = ((i & 1) == 0 && (j & 1) == 0) ? nm[0] : (((i & 1) && (j & 1)) ? nm[1] : nm[2]);
      out[r] = (lev[k] * v) << sh;
    }
  }

  bool residual_luma(H264IMb &m, H264IMb *mL, H264IMb *mT, bool aL, bool aT, int i16mode) {
    (void)i16mode;
    int lev[64];
    if (m.type == 1) {
      // DC
      int a, b;
      a = aL ? (mL->dcflags & 1) : 1;
      b = aT ? (mT->dcflags & 1) : 1;
      int n = residual(0, 85 + 0 + a + 2 * b, 16, lev);
      if (n) {
        m.dcflags |= 1;
        has_dc_l = true;
        int32_t c[16] = {0};
        for (int k = 0; k < 16; k++) c[ZZ4[k]] = lev[k];
        int32_t t[16], f[16];
        for (int i = 0; i < 4; i++) {
          const int32_t *r = c + i * 4;
          t[i * 4 + 0] = r[0] + r[1] + r[2] + r[3];
          t[i * 4 + 1] = r[0] + r[1] - r[2] - r[3];
          t[i * 4 + 2] = r[0] - r[1] - r[2] + r[3];
          t[i * 4 + 3] = r[0] - r[1] + r[2] - r[3];
        }
        for (int j = 0; j < 4; j++) {
          f[0 * 4 + j] = t[j] + t[4 + j] + t[8 + j] + t[12 + j];
          f[1 * 4 + j] = t[j] + t[4 + j] - t[8 + j] - t[12 + j];
          f[2 * 4 + j] = t[j] - t[4 + j] - t[8 + j] + t[12 + j];
          f[3 * 4 + j] = t[j] - t[4 + j] + t[8 + j] - t[12 + j];
        }
        int ls = 16 * NORM4[qp % 6][0], sh = qp / 6;
        for (int i = 0; i < 16; i++) {
          if (qp >= 36) dcl[i] = (f[i] * ls) << (sh - 6);
          else dcl[i] = (f[i] * ls + (1 << (5 - sh))) >> (6 - sh);
        }
      }
      if (m.cbp_l == 15) {
        for (int k = 0; k < 16; k++) {
          int gx = BLK_X[k], gy = BLK_Y[k];
          int cnt = residual(1, 85 + 4 + cbf_inc_luma(m, mL, mT, aL, aT, gx, gy), 15, lev);
          if (cnt) {
            m.cbf_l |= (uint16_t)(1 << (gy * 4 + gx));
            nz4[k] = 1;
            dequant4(c4[k], lev, 1, 15, qp);
          }
        }
      }
      return true;
    }
    for (int b8 = 0; b8 < 4; b8++) {
      if (!((m.cbp_l >> b8) & 1)) continue;
      if (m.t8) {
        int cnt = residual(5, -1, 64, lev);
        if (cnt) {
          nz8[b8] = 1;
          int sh = qp / 6;
          const uint8_t *nm = NORM8[qp % 6];
          for (int k = 0; k < 64; k++) {
            if (!lev[k]) continue;
            int r = ZZ8[k];
            int i = r >> 3, j = r & 7;
            int v;
            if ((i & 3) == 0 && (j & 3) == 0) v = nm[0];
            else if ((i & 1) && (j & 1)) v = nm[1];
            else if ((i & 3) == 2 && (j & 3) == 2) v = nm[2];
            else if (((i & 3) == 0 && (j & 1)) || ((i & 1) && (j & 3) == 0)) v = nm[3];
            else if (((i & 3) == 0 && (j & 3) == 2) || ((i & 3) == 2 && (j & 3) == 0)) v = nm[4];
            else v = nm[5];
            int ls = 16 * v;
            c8[b8][r] = qp >= 36 ? (lev[k] * ls) << (sh - 6) : (lev[k] * ls + (1 << (5 - sh))) >> (6 - sh);
          }
        }
        int gx = (b8 & 1) * 2, gy = (b8 >> 1) * 2;
        if (cnt) m.cbf_l |= (uint16_t)((3 << (gy * 4 + gx)) | (3 << ((gy + 1) * 4 + gx)));
      } else {
        for (int s = 0; s < 4; s++) {
          int k = b8 * 4 + s, gx = BLK_X[k], gy = BLK_Y[k];
          int cnt = residual(2, 85 + 8 + cbf_inc_luma(m, mL, mT, aL, aT, gx, gy), 16, lev);
          if (cnt) {
            m.cbf_l |= (uint16_t)(1 << (gy * 4 + gx));
            nz4[k] = 1;
            dequant4(c4[k], lev, 0, 16, qp);
          }
        }
      }
    }
    return true;
  }

  void residual_chroma(H264IMb &m, H264IMb *mL, H264IMb *mT, bool aL, bool aT) {
    if (!m.cbp_c) return;
    int lev[16];
    for (int c = 0; c < 2; c++) {
      int a = aL ? ((mL->dcflags >> (1 + c)) & 1) : 1;
      int b = aT ? ((mT->dcflags >> (1 + c)) & 1) : 1;
      int n = residual(3, 85 + 12 + a + 2 * b, 4, lev);
      if (n) {
        m.dcflags |= (uint8_t)(2 << c);
        int f00 = lev[0] + lev[1] + lev[2] + lev[3], f01 = lev[0] - lev[1] + lev[2] - lev[3], f10 = lev[0] + lev[1] - lev[2] - lev[3],
            f11 = lev[0] - lev[1] - lev[2] + lev[3];
        int ls = 16 * NORM4[qpc[c] % 6][0], sh = qpc[c] / 6;
        cdc[c][0] = ((f00 * ls) << sh) >> 5;
        cdc[c][1] = ((f01 * ls) << sh) >> 5;
        cdc[c][2] = ((f10 * ls) << sh) >> 5;
        cdc[c][3] = ((f11 * ls) << sh) >> 5;
      }
    }
    if (m.cbp_c != 2) return;
    for (int c = 0; c < 2; c++) {
      uint8_t &mycbf = c ? m.cbf_cr : m.cbf_cb;
      for (int k = 0; k < 4; k++) {
        int gx = k & 1, gy = k >> 1, a, b;
        if (gx > 0) a = (mycbf >> (gy * 2)) & 1;
        else a = aL ? (((c ? mL->cbf_cr : mL->cbf_cb) >> (gy * 2 + 1)) & 1) : 1;
        if (gy > 0) b = (mycbf >> gx) & 1;
        else b = aT ? (((c ? mT->cbf_cr : mT->cbf_cb) >> (2 + gx)) & 1) : 1;
        int cnt = residual(4, 85 + 16 + a + 2 * b, 15, lev);
        if (cnt) {
          mycbf |= (uint8_t)(1 << (gy * 2 + gx));
          cnz[c][k] = 1;
          dequant4(cac[c][k], lev, 1, 15, qpc[c]);
        }
      }
    }
  }

  // ---- reconstruction
  void gather(const uint8_t *pl, int stride, int x, int y, int n, bool hasL, bool hasT, bool hasTL, int ntop, int navail_top, int *top,
              int *left) {
    // top[0] coin, top[1..ntop] ; left[0] coin, left[1..n]
    if (hasTL) top[0] = left[0] = pl[(y - 1) * stride + x - 1];
    else top[0] = left[0] = 128;
    if (hasT) {
      for (int i = 0; i < navail_top; i++) top[1 + i] = pl[(y - 1) * stride + x + i];
      for (int i = navail_top; i < ntop; i++) top[1 + i] = top[navail_top];
    } else {
      for (int i = 0; i < ntop; i++) top[1 + i] = 128;
    }
    if (hasL)
      for (int i = 0; i < n; i++) left[1 + i] = pl[(y + i) * stride + x - 1];
    else
      for (int i = 0; i < n; i++) left[1 + i] = 128;
  }

  void pred_i4(uint8_t *Y, int ys, int px, int py, int mode, bool aL, bool aT, bool aTR, bool aTL, int gx, int gy, int k) {
    bool hasL = gx > 0 || aL, hasT = gy > 0 || aT;
    bool hasTL = (gx > 0 && gy > 0) ? true : (gx > 0 ? aT : (gy > 0 ? aL : aTL));
    bool hasTR;
    if (gy == 0) hasTR = gx < 3 ? aT : aTR;
    else {
      int ok = -1;
      if (gx < 3) {
        int nx = gx + 1, ny = gy - 1;
        int nk = ((ny >> 1) * 2 + (nx >> 1)) * 4 + (ny & 1) * 2 + (nx & 1);
        ok = nk < k;
      }
      hasTR = ok == 1;
    }
    int top[9], left[5];
    gather(Y, ys, px, py, 4, hasL, hasT, hasTL, 8, hasTR ? 8 : 4, top, left);
    // si le haut n'existe pas, les echantillons 4..7 restent 128 (gather)
    pred_nxn(Y + py * ys + px, ys, 4, mode, top, left, hasL, hasT);
  }

  void pred_i8(uint8_t *Y, int ys, int px, int py, int mode, bool aL, bool aT, bool aTR, bool aTL, int b8) {
    int bx = b8 & 1, by = b8 >> 1;
    bool hasL = bx > 0 || aL, hasT = by > 0 || aT;
    bool hasTL = (bx > 0 && by > 0) ? true : (bx > 0 ? aT : (by > 0 ? aL : aTL));
    bool hasTR = by == 0 ? (bx == 0 ? aT : aTR) : (bx == 0);
    int top[17], left[9];
    gather(Y, ys, px, py, 8, hasL, hasT, hasTL, 16, hasTR ? 16 : 8, top, left);
    // filtrage des echantillons de reference (8.3.2.2.1)
    int ft[17], fl[9];
    bool tl = hasTL;
    if (hasT) {
      ft[1] = tl ? (top[0] + 2 * top[1] + top[2] + 2) >> 2 : (3 * top[1] + top[2] + 2) >> 2;
      for (int x = 1; x < 15; x++) ft[1 + x] = (top[x] + 2 * top[x + 1] + top[x + 2] + 2) >> 2;
      ft[16] = (top[15] + 3 * top[16] + 2) >> 2;
    } else
      for (int x = 0; x < 16; x++) ft[1 + x] = 128;
    if (hasTL) {
      if (!hasT || !hasL) {
        if (hasT) ft[0] = (3 * top[0] + top[1] + 2) >> 2;
        else if (hasL) ft[0] = (3 * top[0] + left[1] + 2) >> 2;
        else ft[0] = top[0];
      } else ft[0] = (top[1] + 2 * top[0] + left[1] + 2) >> 2;
    } else ft[0] = 128;
    if (hasL) {
      fl[1] = tl ? (top[0] + 2 * left[1] + left[2] + 2) >> 2 : (3 * left[1] + left[2] + 2) >> 2;
      for (int y = 1; y < 7; y++) fl[1 + y] = (left[y] + 2 * left[y + 1] + left[y + 2] + 2) >> 2;
      fl[8] = (left[7] + 3 * left[8] + 2) >> 2;
    } else
      for (int y = 0; y < 8; y++) fl[1 + y] = 128;
    fl[0] = ft[0];
    pred_nxn(Y + py * ys + px, ys, 8, mode, ft, fl, hasL, hasT);
  }

  void reconstruct(H264IMb &m, int mx, int my, bool aL, bool aT, bool aTR, bool aTL, int i16mode) {
    uint8_t *Y = D.pl_[0];
    int ys = D.mbw_ * 16, x0 = mx * 16, y0 = my * 16;
    if (m.type == 1) {
      int top[17], left[17];
      gather(Y, ys, x0, y0, 16, aL, aT, aTL, 16, 16, top, left);
      uint8_t *dst = Y + y0 * ys + x0;
      if (i16mode == 3) {
        int H = 0, V = 0;
        for (int i = 0; i < 8; i++) {
          H += (i + 1) * (top[1 + 8 + i] - top[1 + 6 - i]);
          V += (i + 1) * (left[1 + 8 + i] - left[1 + 6 - i]);
        }
        int a = 16 * (left[16] + top[16]), b = (5 * H + 32) >> 6, c = (5 * V + 32) >> 6;
        for (int y = 0; y < 16; y++)
          for (int x = 0; x < 16; x++) dst[y * ys + x] = (uint8_t)clip255((a + b * (x - 7) + c * (y - 7) + 16) >> 5);
      } else {
        for (int y = 0; y < 16; y++)
          for (int x = 0; x < 16; x++) {
            int v;
            if (i16mode == 0) v = top[1 + x];
            else if (i16mode == 1) v = left[1 + y];
            else {
              int s = 0;
              if (aL && aT) {
                for (int i = 0; i < 16; i++) s += top[1 + i] + left[1 + i];
                v = (s + 16) >> 5;
              } else if (aL) {
                for (int i = 0; i < 16; i++) s += left[1 + i];
                v = (s + 8) >> 4;
              } else if (aT) {
                for (int i = 0; i < 16; i++) s += top[1 + i];
                v = (s + 8) >> 4;
              } else v = 128;
            }
            dst[y * ys + x] = (uint8_t)v;
          }
      }
      for (int k = 0; k < 16; k++) {
        int gx = BLK_X[k], gy = BLK_Y[k];
        int32_t *c = c4[k];
        c[0] = dcl[gy * 4 + gx];
        if (nz4[k] || c[0]) idct4_add(Y + (y0 + gy * 4) * ys + x0 + gx * 4, ys, c);
      }
    } else if (m.t8) {
      for (int b8 = 0; b8 < 4; b8++) {
        int gx = (b8 & 1) * 2, gy = (b8 >> 1) * 2;
        int px = x0 + gx * 4, py = y0 + gy * 4;
        pred_i8(Y, ys, px, py, m.pm[gy * 4 + gx], aL, aT, aTR, aTL, b8);
        if (nz8[b8]) idct8_add(Y + py * ys + px, ys, c8[b8]);
      }
    } else {
      for (int k = 0; k < 16; k++) {
        int gx = BLK_X[k], gy = BLK_Y[k];
        int px = x0 + gx * 4, py = y0 + gy * 4;
        pred_i4(Y, ys, px, py, m.pm[gy * 4 + gx], aL, aT, aTR, aTL, gx, gy, k);
        if (nz4[k]) idct4_add(Y + py * ys + px, ys, c4[k]);
      }
    }
    // chroma
    int cs = D.mbw_ * 8, cx = mx * 8, cy = my * 8;
    for (int c = 0; c < 2; c++) {
      uint8_t *P = D.pl_[1 + c];
      int top[9], left[9];
      gather(P, cs, cx, cy, 8, aL, aT, aTL, 8, 8, top, left);
      uint8_t *dst = P + cy * cs + cx;
      int mode = m.chroma_mode;
      if (mode == 3) {
        int H = 0, V = 0;
        for (int i = 0; i < 4; i++) {
          H += (i + 1) * (top[1 + 4 + i] - top[1 + 2 - i]);
          V += (i + 1) * (left[1 + 4 + i] - left[1 + 2 - i]);
        }
        int a = 16 * (left[8] + top[8]), b = (34 * H + 32) >> 6, cc = (34 * V + 32) >> 6;
        for (int y = 0; y < 8; y++)
          for (int x = 0; x < 8; x++) dst[y * cs + x] = (uint8_t)clip255((a + b * (x - 3) + cc * (y - 3) + 16) >> 5);
      } else if (mode == 1) {
        for (int y = 0; y < 8; y++)
          for (int x = 0; x < 8; x++) dst[y * cs + x] = (uint8_t)left[1 + y];
      } else if (mode == 2) {
        for (int y = 0; y < 8; y++)
          for (int x = 0; x < 8; x++) dst[y * cs + x] = (uint8_t)top[1 + x];
      } else {
        for (int blk = 0; blk < 4; blk++) {
          int bx = blk & 1, by = blk >> 1;
          int st = 0, sl = 0;
          for (int i = 0; i < 4; i++) {
            st += top[1 + bx * 4 + i];
            sl += left[1 + by * 4 + i];
          }
          int v;
          if (bx == by) {  // blocs 0 et 3
            if (aL && aT) v = (st + sl + 4) >> 3;
            else if (aL) v = (sl + 2) >> 2;
            else if (aT) v = (st + 2) >> 2;
            else v = 128;
          } else if (bx == 1) {  // bloc 1
            if (aT) v = (st + 2) >> 2;
            else if (aL) v = (sl + 2) >> 2;
            else v = 128;
          } else {  // bloc 2
            if (aL) v = (sl + 2) >> 2;
            else if (aT) v = (st + 2) >> 2;
            else v = 128;
          }
          for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++) dst[(by * 4 + y) * cs + bx * 4 + x] = (uint8_t)v;
        }
      }
      if (m.cbp_c) {
        for (int k = 0; k < 4; k++) {
          int32_t *cf = cac[c][k];
          cf[0] = cdc[c][k];
          if (cnz[c][k] || cf[0]) idct4_add(P + (cy + (k >> 1) * 4) * cs + cx + (k & 1) * 4, cs, cf);
        }
      }
    }
  }
};

// =====================================================================
//  Decodeur
// =====================================================================
H264Intra::H264Intra(h264i_alloc_fn a, h264i_free_fn f) : alloc_(a), free_(f) {
  if (!zz_ready) {
    make_zigzag(ZZ4, 4);
    make_zigzag(ZZ8, 8);
    zz_ready = true;
  }
}

H264Intra::~H264Intra() {
  for (int i = 0; i < 3; i++) release(pl_[i]);
  release(mb_);
  release(rbsp_);
}

void *H264Intra::alloc(size_t n) { return alloc_ ? alloc_(n) : malloc(n); }
void H264Intra::release(void *p) {
  if (!p) return;
  if (free_) free_(p);
  else free(p);
}

bool H264Intra::fail(const char *m) {
  err_ = m;
  return false;
}

int H264Intra::parse_sps(const uint8_t *r, size_t n) {
  if (n < 4) return -1;
  Sps s;
  s.profile = r[0];
  Bits b{r + 3, n - 3};
  b.ue();
  switch (s.profile) {
    case 100: case 110: case 122: case 244: case 44: case 83: case 86: case 118: case 128: case 138: case 139: case 134: case 135: {
      s.chroma = (int)b.ue();
      if (s.chroma == 3) b.u1();
      uint32_t bdl = b.ue(), bdc = b.ue();
      b.u1();
      if (b.u1()) {
        s.scaling = true;
        int cnt = (s.chroma != 3) ? 8 : 12;
        for (int i = 0; i < cnt; i++)
          if (b.u1()) skip_scaling(b, i < 6 ? 16 : 64);
      }
      if (bdl != 0 || bdc != 0) return fail("profondeur de bits > 8 non prise en charge") ? 0 : -1;
      break;
    }
    default: break;
  }
  s.log2_fn = (int)b.ue() + 4;
  s.poc_type = (int)b.ue();
  if (s.poc_type == 0) s.log2_poc = (int)b.ue() + 4;
  else if (s.poc_type == 1) {
    s.delta_zero = b.u1();
    b.se();
    b.se();
    uint32_t k = b.ue();
    for (uint32_t i = 0; i < k && i < 255; i++) b.se();
  }
  b.ue();
  b.u1();
  s.mbw = (int)b.ue() + 1;
  s.mbh = (int)b.ue() + 1;
  s.frame_mbs_only = b.u1();
  if (!s.frame_mbs_only) {
    b.u1();
    return fail("entrelace non pris en charge") ? 0 : -1;
  }
  b.u1();
  if (b.u1()) {
    s.crop_l = (int)b.ue();
    s.crop_r = (int)b.ue();
    s.crop_t = (int)b.ue();
    s.crop_b = (int)b.ue();
  }
  if (s.chroma != 1) {
    err_ = "seul le 4:2:0 est pris en charge";
    return -1;
  }
  if (s.mbw < 1 || s.mbh < 1 || s.mbw > 256 || s.mbh > 256) {
    err_ = "dimensions invalides";
    return -1;
  }
  s.ok = true;
  sps_ = s;
  return 0;
}

int H264Intra::parse_pps(const uint8_t *r, size_t n) {
  Bits b{r, n};
  Pps p;
  b.ue();
  p.sps_id = (int)b.ue();
  p.cabac = b.u1();
  p.poc_present = b.u1();
  p.slice_groups = (int)b.ue();
  b.ue();
  b.ue();
  b.u1();
  b.u(2);
  p.init_qp = 26 + b.se();
  b.se();
  p.cqp_off = b.se();
  p.deblock_ctrl = b.u1();
  p.constrained = b.u1();
  p.redundant = b.u1();
  p.cqp_off2 = p.cqp_off;
  // position du bit d'arret final
  size_t last = n;
  while (last > 0 && r[last - 1] == 0) last--;
  size_t stop = 0;
  if (last > 0) {
    int tz = 0;
    while (!((r[last - 1] >> tz) & 1)) tz++;
    stop = (last - 1) * 8 + (7 - tz);
  }
  if (b.pos < stop) {
    p.t8 = b.u1();
    if (b.u1()) {
      p.scaling = true;
      int cnt = 6 + (p.t8 ? 2 : 0);
      for (int i = 0; i < cnt; i++)
        if (b.u1()) skip_scaling(b, i < 6 ? 16 : 64);
    }
    p.cqp_off2 = b.se();
  }
  if (p.slice_groups != 0) {
    err_ = "groupes de tranches non pris en charge";
    return -1;
  }
  p.ok = true;
  pps_ = p;
  return 0;
}

bool H264Intra::setup_picture() {
  if (!sps_.ok) return fail("SPS manquant");
  int w = sps_.mbw, h = sps_.mbh;
  if (w != mbw_ || h != mbh_ || !pl_[0]) {
    for (int i = 0; i < 3; i++) {
      release(pl_[i]);
      pl_[i] = nullptr;
    }
    release(mb_);
    mb_ = nullptr;
    mbw_ = w;
    mbh_ = h;
    pl_[0] = (uint8_t *)alloc((size_t)w * h * 256);
    pl_[1] = (uint8_t *)alloc((size_t)w * h * 64);
    pl_[2] = (uint8_t *)alloc((size_t)w * h * 64);
    mb_ = (H264IMb *)alloc(sizeof(H264IMb) * (size_t)w * h);
    if (!pl_[0] || !pl_[1] || !pl_[2] || !mb_) return fail("memoire insuffisante");
  }
  crop_w_ = w * 16 - 2 * (sps_.crop_l + sps_.crop_r);
  crop_h_ = h * 16 - 2 * (sps_.crop_t + sps_.crop_b);
  mb_total_ = w * h;
  mbs_done_ = 0;
  slice_no_ = 0;
  for (int i = 0; i < mb_total_; i++) mb_[i].slice = -1;
  return true;
}

int H264Intra::decode_slice(const uint8_t *r, size_t n) {
  Bits b{r, n};
  int first_mb = (int)b.ue();
  int stype = (int)b.ue() % 5;
  b.ue();  // pps_id
  if (!pps_.ok || !sps_.ok) return fail("SPS/PPS manquant") ? 0 : -1;
  if (stype != 2) return fail("tranche non I") ? 0 : -1;
  if (!pps_.cabac) return fail("CAVLC non pris en charge") ? 0 : -1;
  if (sps_.scaling || pps_.scaling) return fail("matrices de quantification non prises en charge") ? 0 : -1;
  b.u(sps_.log2_fn);  // frame_num
  b.ue();             // idr_pic_id
  if (sps_.poc_type == 0) {
    b.u(sps_.log2_poc);
    if (pps_.poc_present) b.se();
  } else if (sps_.poc_type == 1 && !sps_.delta_zero) {
    b.se();
    if (pps_.poc_present) b.se();
  }
  if (pps_.redundant) b.ue();
  b.u1();  // no_output_of_prior_pics
  b.u1();  // long_term_reference
  int dqp = b.se();
  if (pps_.deblock_ctrl) {
    if (b.ue() != 1) {
      b.se();
      b.se();
    }
  }
  if (first_mb == 0 || mbs_done_ >= mb_total_ || !mb_) {
    if (!setup_picture()) return -1;
  }
  b.pos = (b.pos + 7) & ~(size_t)7;
  if (b.pos / 8 >= n) return fail("tranche vide") ? 0 : -1;

  H264ISlice S(*this);
  S.sid = ++slice_no_;
  S.qp = pps_.init_qp + dqp;
  S.t8mode = pps_.t8;
  S.cqp_off[0] = pps_.cqp_off;
  S.cqp_off[1] = pps_.cqp_off2;
  S.cb.start(r, n, b.pos / 8);
  S.cb.init_ctx(S.qp);
  int addr = first_mb;
  for (;;) {
    if (addr >= mb_total_) return fail("tranche hors image") ? 0 : -1;
    if (!S.decode_mb(addr)) return -1;
    mbs_done_++;
    addr++;
    if (S.cb.term()) break;
    if (addr >= mb_total_) break;
  }
  return 0;
}

int H264Intra::feed(const uint8_t *nal, size_t len) {
  if (len < 2) return 1;
  int type = nal[0] & 0x1F;
  if (type != 5 && type != 7 && type != 8) return 1;
  if (rbsp_cap_ < len) {
    release(rbsp_);
    rbsp_ = (uint8_t *)alloc(len + 64);
    if (!rbsp_) {
      rbsp_cap_ = 0;
      err_ = "memoire insuffisante";
      return -1;
    }
    rbsp_cap_ = len;
  }
  size_t n = unescape(nal + 1, len - 1, rbsp_);
  err_ = "";
  int r;
  if (type == 7) r = parse_sps(rbsp_, n);
  else if (type == 8) r = parse_pps(rbsp_, n);
  else r = decode_slice(rbsp_, n);
  return (r == 0 && *err_) ? -1 : r;
}
