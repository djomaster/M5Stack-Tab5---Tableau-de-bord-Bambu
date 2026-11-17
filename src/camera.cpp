// Sonde caméra : se connecte au flux RTSPS d'une imprimante Bambu (X1C, H2C...) et détermine si le flux
// est lisible par le décodeur H.264 logiciel du P4 (esp_h264 : profil "constrained baseline" seulement).
// Aucune image n'est décodée ni affichée ici : on mesure le profil, le CABAC, les images B et la cadence.
#include "camera.h"
#include <WiFiClientSecure.h>
#include <mbedtls/base64.h>
#include <mbedtls/md5.h>
#include <stdarg.h>
#include <esp_timer.h>
#include "bambu.h"
#include "h264i.h"
#include "tls_gate.h"

static SemaphoreHandle_t g_mx = nullptr;
static String g_report;
static volatile bool g_running = false;
static volatile bool g_stop = false;
static bool g_view = false;  // vrai : mode image en direct ; faux : sonde de diagnostic
static String g_last_line;

static const uint16_t RTSP_PORT = 322;
static const uint32_t PLAY_MS = 8000;
static const uint32_t VIEW_PERIOD_MS = 1000;  // une image decodee par seconde (une image cle par seconde dans le flux)
static const uint32_t VIEW_TOLERANCE_MS = 150;  // gigue de l'arrivee des images cles
static const int VIEW_MAXW = 900, VIEW_MAXH = 560;

// ---------------------------------------------------------------- compte rendu
static void rep(const char *fmt, ...) {
  char line[170];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(line, sizeof(line), fmt, ap);
  va_end(ap);
  Serial.printf("Camera : %s\n", line);
  xSemaphoreTake(g_mx, portMAX_DELAY);
  g_last_line = line;
  if (g_report.length() < 2200) {
    g_report += line;
    g_report += "\n";
  }
  xSemaphoreGive(g_mx);
}

String camera_probe_report() {
  if (!g_mx) return "";
  xSemaphoreTake(g_mx, portMAX_DELAY);
  String r = g_report;
  xSemaphoreGive(g_mx);
  return r;
}

bool camera_probe_running() { return g_running; }

// ---------------------------------------------------------------- lecture de bits H.264
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

static void skip_scaling(Bits &b, int size) {
  int last = 8, next = 8;
  for (int j = 0; j < size; j++) {
    if (next != 0) next = (last + b.se() + 256) % 256;
    last = (next == 0) ? last : next;
  }
}

struct SpsInfo {
  bool ok = false;
  int profile = 0, constraint = 0, level = 0, w = 0, h = 0;
};
static SpsInfo g_sps;
static int g_cabac = -1;  // -1 inconnu, 0 CAVLC, 1 CABAC

// Retire les octets d'échappement 0x03 d'une unité NAL (hors octet d'en-tête).
static size_t unescape(const uint8_t *in, size_t n, uint8_t *out, size_t cap) {
  size_t o = 0;
  int zeros = 0;
  for (size_t i = 0; i < n && o < cap; i++) {
    if (zeros >= 2 && in[i] == 3) {
      zeros = 0;
      continue;
    }
    zeros = (in[i] == 0) ? zeros + 1 : 0;
    out[o++] = in[i];
  }
  return o;
}

// 'nal' commence par l'octet d'en-tête (type 7).
static void parse_sps(const uint8_t *nal, size_t len) {
  if (len < 5) return;
  uint8_t buf[96];
  size_t n = unescape(nal + 1, len - 1 > sizeof(buf) ? sizeof(buf) : len - 1, buf, sizeof(buf));
  if (n < 4) return;
  SpsInfo s;
  s.profile = buf[0];
  s.constraint = buf[1];
  s.level = buf[2];
  Bits b{buf + 3, n - 3};
  b.ue();  // sps_id
  int chroma = 1;
  switch (s.profile) {
    case 100: case 110: case 122: case 244: case 44: case 83: case 86: case 118: case 128: case 138: case 139: case 134: case 135: {
      chroma = (int)b.ue();
      if (chroma == 3) b.u1();
      b.ue();
      b.ue();
      b.u1();
      if (b.u1()) {
        int cnt = (chroma != 3) ? 8 : 12;
        for (int i = 0; i < cnt; i++)
          if (b.u1()) skip_scaling(b, i < 6 ? 16 : 64);
      }
      break;
    }
    default: break;
  }
  b.ue();  // log2_max_frame_num
  uint32_t poc = b.ue();
  if (poc == 0) {
    b.ue();
  } else if (poc == 1) {
    b.u1();
    b.se();
    b.se();
    uint32_t k = b.ue();
    for (uint32_t i = 0; i < k && i < 255; i++) b.se();
  }
  b.ue();  // max_num_ref_frames
  b.u1();  // gaps
  int wm = (int)b.ue() + 1;
  int hm = (int)b.ue() + 1;
  int fmo = (int)b.u1();
  if (!fmo) b.u1();
  b.u1();  // direct_8x8
  int cl = 0, cr = 0, ct = 0, cb = 0;
  if (b.u1()) {
    cl = (int)b.ue();
    cr = (int)b.ue();
    ct = (int)b.ue();
    cb = (int)b.ue();
  }
  int cux = (chroma == 1 || chroma == 2) ? 2 : 1;
  int cuy = ((chroma == 1) ? 2 : 1) * (2 - fmo);
  s.w = wm * 16 - cux * (cl + cr);
  s.h = (2 - fmo) * hm * 16 - cuy * (ct + cb);
  s.ok = (s.w > 0 && s.h > 0 && s.w <= 8192 && s.h <= 8192);
  g_sps = s;
}

static void parse_pps(const uint8_t *nal, size_t len) {
  if (len < 3) return;
  uint8_t buf[32];
  size_t n = unescape(nal + 1, len - 1 > sizeof(buf) ? sizeof(buf) : len - 1, buf, sizeof(buf));
  Bits b{buf, n};
  b.ue();
  b.ue();
  g_cabac = (int)b.u1();
}

// ---------------------------------------------------------------- decodage d'essai (chronometrage sur la tablette)
static const uint32_t MAX_PICS = 3;
static H264Intra *g_dec = nullptr;
static uint8_t *g_nal = nullptr;
static size_t g_nal_len = 0, g_nal_cap = 0;
static bool g_fu_open = false;
static uint32_t g_pics = 0, g_dec_errs = 0, g_pic_ms[MAX_PICS];
static int64_t g_pic_us = 0;
static const char *g_dec_err = "";
// file d'attente du mode image en direct (voir view_offer)
static uint8_t *g_job_buf = nullptr;
static size_t g_job_len = 0;
static uint8_t g_job_sps[256], g_job_pps[256], g_cur_sps[256], g_cur_pps[256];
static size_t g_job_sps_len = 0, g_job_pps_len = 0, g_cur_sps_len = 0, g_cur_pps_len = 0;
static volatile int g_job_state = 0;  // 0 : decodeur libre, 1 : une image attend ou est en cours de decodage
static volatile bool g_dec_quit = false, g_dec_done = true;

static void *psram_alloc(size_t n) { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
static void psram_free(void *p) { heap_caps_free(p); }

// Libere le decodeur et le tampon de reassemblage a la sortie de la sonde
struct DecGuard {
  DecGuard() {
    g_pics = g_dec_errs = 0;
    g_pic_us = 0;
    g_fu_open = false;
    g_nal_len = 0;
    g_dec_err = "";
    g_nal_cap = 400 * 1024;
    g_nal = (uint8_t *)psram_alloc(g_nal_cap);
    g_job_buf = (uint8_t *)psram_alloc(g_nal_cap);
    g_job_state = 0;
    g_cur_sps_len = g_cur_pps_len = 0;
    g_dec = new H264Intra(psram_alloc, psram_free);
  }
  ~DecGuard() {
    delete g_dec;
    g_dec = nullptr;
    psram_free(g_nal);
    g_nal = nullptr;
    psram_free(g_job_buf);
    g_job_buf = nullptr;
  }
};

// ---------------------------------------------------------------- image en direct (RGB565 reduite 2 fois, double tampon)
static uint16_t *g_vbuf[2] = {nullptr, nullptr};  // alloues une fois, jamais liberes (l'interface peut encore les afficher)
static volatile int g_front = 0;
static volatile int g_vw = 0, g_vh = 0;
static volatile uint32_t g_vseq = 0, g_vms = 0;
static uint32_t g_last_pic_ms = 0;
static bool g_want = false;

static inline int clamp8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

// Reduit l'image decodee 2 fois (moyenne 2x2) et la convertit en RGB565 dans le tampon arriere, puis l'echange avec l'avant.
static void view_publish() {
  if (!g_vbuf[0]) return;
  int ow = g_dec->width() / 2, oh = g_dec->height() / 2;
  if (ow < 8 || oh < 8 || ow > VIEW_MAXW || oh > VIEW_MAXH) return;
  int back = 1 - g_front;
  uint16_t *o = g_vbuf[back];
  const uint8_t *Y = g_dec->plane(0), *U = g_dec->plane(1), *V = g_dec->plane(2);
  int ys = g_dec->stride(0), cs = g_dec->stride(1);
  for (int y = 0; y < oh; y++) {
    for (int x = 0; x < ow; x++) {
      const uint8_t *r0 = Y + (y * 2) * ys + x * 2;
      int sy = r0[0] + r0[1] + r0[ys] + r0[ys + 1];
      int ci = y * cs + x;
      int yy = (((sy >> 2) - 16) * 298) >> 8, u = U[ci] - 128, v = V[ci] - 128;
      int rr = clamp8(yy + ((409 * v) >> 8));
      int gg = clamp8(yy - ((100 * u + 208 * v) >> 8));
      int bb = clamp8(yy + ((516 * u) >> 8));
      o[y * ow + x] = (uint16_t)(((rr & 0xF8) << 8) | ((gg & 0xFC) << 3) | (bb >> 3));
    }
  }
  g_vw = ow;
  g_vh = oh;
  g_front = back;
  g_vseq = g_vseq + 1;
}

// Une unite NAL complete (sans code de debut) : SPS, PPS et tranches IDR vont au decodeur, les 3 premieres images sont chronometrees.
// Mode image en direct : le decodage (~1 s) ne doit pas bloquer la lecture du flux, sinon l'imprimante finit par ne plus rien envoyer.
// Le lecteur ne fait que copier une image cle (avec le dernier SPS/PPS) dans g_job_buf ; une tache separee la decode.

static void view_offer(const uint8_t *nal, size_t len, int t) {
  if (t == 7 && len <= sizeof(g_cur_sps)) {
    memcpy(g_cur_sps, nal, len);
    g_cur_sps_len = len;
  } else if (t == 8 && len <= sizeof(g_cur_pps)) {
    memcpy(g_cur_pps, nal, len);
    g_cur_pps_len = len;
  } else if (t == 5) {
    Bits b{nal + 1, len - 1 > 16 ? 16 : len - 1};
    if (b.ue() != 0) return;  // les flux Bambu n'ont qu'une tranche par image
    if (g_job_state != 0 || !g_job_buf || len > g_nal_cap || !g_cur_sps_len || !g_cur_pps_len) return;
    // delai mesure entre les arrivees des images cles retenues
    if (millis() - g_last_pic_ms < VIEW_PERIOD_MS - VIEW_TOLERANCE_MS) return;
    memcpy(g_job_buf, nal, len);
    g_job_len = len;
    memcpy(g_job_sps, g_cur_sps, g_cur_sps_len);
    g_job_sps_len = g_cur_sps_len;
    memcpy(g_job_pps, g_cur_pps, g_cur_pps_len);
    g_job_pps_len = g_cur_pps_len;
    g_last_pic_ms = millis();
    g_job_state = 1;
  }
}

static void dec_task(void *) {
  while (!g_dec_quit) {
    if (g_job_state != 1) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    int64_t t0 = esp_timer_get_time();
    g_dec->feed(g_job_sps, g_job_sps_len);
    g_dec->feed(g_job_pps, g_job_pps_len);
    int r = g_dec->feed(g_job_buf, g_job_len);
    if (r < 0) {
      g_dec_errs++;
      g_dec_err = g_dec->last_error();
    } else if (g_dec->picture_ready()) {
      view_publish();
      g_vms = (uint32_t)((esp_timer_get_time() - t0) / 1000);
    }
    g_job_state = 0;
  }
  g_dec_done = true;
  vTaskDelete(nullptr);
}

// Une unite NAL complete (sans code de debut) : en mode sonde, SPS, PPS et tranches IDR vont au decodeur, les 3 premieres images sont chronometrees.
static void nal_complete(const uint8_t *nal, size_t len) {
  if (!g_dec || len < 2) return;
  int t = nal[0] & 0x1F;
  if (t != 5 && t != 7 && t != 8) return;
  if (g_view) {
    view_offer(nal, len, t);
    return;
  }
  if (g_pics >= MAX_PICS) return;
  int64_t t0 = esp_timer_get_time();
  int r = g_dec->feed(nal, len);
  int64_t dt = esp_timer_get_time() - t0;
  if (r < 0) {
    g_dec_errs++;
    g_dec_err = g_dec->last_error();
    return;
  }
  if (t == 5) {
    g_pic_us += dt;
    if (g_dec->picture_ready()) {
      g_pic_ms[g_pics++] = (uint32_t)(g_pic_us / 1000);
      g_pic_us = 0;
    }
  }
}

// ---------------------------------------------------------------- statistiques du flux
static uint32_t st_frames, st_bytes, st_idr, st_i, st_p, st_b, st_pkts, st_lost;
static int32_t st_last_seq;

// 'body' : octets qui suivent l'en-tête NAL.
static void on_nal(int type, const uint8_t *body, size_t blen, const uint8_t *whole, size_t wlen) {
  if (type == 7) {
    if (!g_sps.ok) parse_sps(whole, wlen);
  } else if (type == 8) {
    if (g_cabac < 0) parse_pps(whole, wlen);
  } else if (type == 1 || type == 5) {
    if (type == 5) st_idr++;
    Bits b{body, blen > 16 ? 16 : blen};
    b.ue();  // first_mb_in_slice
    uint32_t t = b.ue() % 5;
    if (t == 0) st_p++;
    else if (t == 1) st_b++;
    else st_i++;
  }
}

static void handle_rtp(const uint8_t *p, size_t len) {
  if (len < 13 || (p[0] >> 6) != 2) return;
  st_pkts++;
  int32_t seq = (p[2] << 8) | p[3];
  if (st_last_seq >= 0 && seq != ((st_last_seq + 1) & 0xFFFF)) {
    st_lost++;
    g_fu_open = false;  // une trame perdue invalide l'unite NAL fragmentee en cours
  }
  st_last_seq = seq;
  size_t off = 12 + 4 * (p[0] & 0x0F);
  if (p[0] & 0x10) {
    if (off + 4 > len) return;
    off += 4 + 4 * (size_t)((p[off + 2] << 8) | p[off + 3]);
  }
  if (off >= len) return;
  const uint8_t *n = p + off;
  size_t nl = len - off;
  st_bytes += nl;
  if (p[1] & 0x80) st_frames++;  // bit marqueur : fin d'une image
  int type = n[0] & 0x1F;
  if (type >= 1 && type <= 23) {
    on_nal(type, n + 1, nl - 1, n, nl);
    nal_complete(n, nl);
  } else if (type == 24) {  // STAP-A : plusieurs NAL
    size_t i = 1;
    while (i + 2 < nl) {
      size_t sz = (n[i] << 8) | n[i + 1];
      i += 2;
      if (sz == 0 || i + sz > nl) break;
      on_nal(n[i] & 0x1F, n + i + 1, sz - 1, n + i, sz);
      nal_complete(n + i, sz);
      i += sz;
    }
  } else if (type == 28 && nl > 2) {  // FU-A : on ne lit que le début de chaque NAL fragmenté
    if (n[1] & 0x80) {
      on_nal(n[1] & 0x1F, n + 2, nl - 2, n + 1, nl - 1);
      g_nal_len = 0;
      g_fu_open = g_nal != nullptr;
      if (g_fu_open) g_nal[g_nal_len++] = (uint8_t)((n[0] & 0xE0) | (n[1] & 0x1F));
    }
    if (g_fu_open) {
      size_t add = nl - 2;
      if (g_nal_len + add <= g_nal_cap) {
        memcpy(g_nal + g_nal_len, n + 2, add);
        g_nal_len += add;
      } else g_fu_open = false;
      if (g_fu_open && (n[1] & 0x40)) {
        nal_complete(g_nal, g_nal_len);
        g_fu_open = false;
      }
    }
  }
}

// ---------------------------------------------------------------- client RTSP
static String g_pass, g_realm, g_nonce;
static bool g_digest = false, g_basic = false;
static int g_cseq = 0;
static uint32_t g_skipped = 0;

// Trace une réponse RTSP sur une ligne (retours à la ligne remplacés, 150 caractères au plus).
static void rep_hdr(const char *label, const String &h) {
  String t;
  for (size_t i = 0; i < h.length() && t.length() < 150; i++) {
    char ch = h[i];
    if (ch == '\r') continue;
    t += (ch == '\n') ? '|' : ((ch >= 32 && ch < 127) ? ch : '?');
  }
  rep("%s : %s", label, t.c_str());
}

static String md5hex(const String &s) {
  uint8_t o[16];
  mbedtls_md5((const unsigned char *)s.c_str(), s.length(), o);
  char h[33];
  for (int i = 0; i < 16; i++) sprintf(h + 2 * i, "%02x", o[i]);
  return String(h);
}

static String auth_for(const String &method, const String &uri) {
  if (g_digest) {
    String ha1 = md5hex("bblp:" + g_realm + ":" + g_pass);
    String ha2 = md5hex(method + ":" + uri);
    String resp = md5hex(ha1 + ":" + g_nonce + ":" + ha2);
    return "Authorization: Digest username=\"bblp\", realm=\"" + g_realm + "\", nonce=\"" + g_nonce + "\", uri=\"" + uri +
           "\", response=\"" + resp + "\"\r\n";
  }
  if (g_basic) {
    String cred = "bblp:" + g_pass;
    unsigned char out[96];
    size_t ol = 0;
    mbedtls_base64_encode(out, sizeof(out), &ol, (const unsigned char *)cred.c_str(), cred.length());
    String b64;
    b64.concat((const char *)out, ol);
    return "Authorization: Basic " + b64 + "\r\n";
  }
  return "";
}

static String qv(const String &s, const char *key) {
  int i = s.indexOf(key);
  if (i < 0) return "";
  i += strlen(key);
  if (i < (int)s.length() && s[i] == '"') {
    int j = s.indexOf('"', i + 1);
    return j < 0 ? "" : s.substring(i + 1, j);
  }
  int j = i;
  while (j < (int)s.length() && s[j] != ',' && s[j] != '\r' && s[j] != '\n' && s[j] != ';') j++;
  return s.substring(i, j);
}

static void parse_auth(const String &hdr) {
  int pos = 0;
  bool dig = false, bas = false;
  String dl;
  while (pos < (int)hdr.length()) {
    int e = hdr.indexOf('\n', pos);
    if (e < 0) e = hdr.length();
    String line = hdr.substring(pos, e);
    pos = e + 1;
    String l = line;
    l.toLowerCase();
    if (l.startsWith("www-authenticate:")) {
      if (l.indexOf("digest") >= 0) {
        dig = true;
        dl = line;
      } else if (l.indexOf("basic") >= 0) {
        bas = true;
      }
    }
  }
  if (dig) {
    g_digest = true;
    g_basic = false;
    g_realm = qv(dl, "realm=");
    g_nonce = qv(dl, "nonce=");
  } else if (bas) {
    g_basic = true;
    g_digest = false;
  }
}

static bool rd(WiFiClientSecure &c, uint8_t *buf, size_t n, uint32_t to_ms) {
  size_t got = 0;
  uint32_t t0 = millis();
  while (got < n) {
    int av = c.available();
    if (av > 0) {
      int k = c.read(buf + got, (size_t)av < n - got ? (size_t)av : n - got);
      if (k > 0) got += k;
      continue;
    }
    if (!c.connected() && c.available() <= 0) return false;
    if (millis() - t0 > to_ms) return false;
    vTaskDelay(pdMS_TO_TICKS(3));
  }
  return true;
}

struct Resp {
  int code = 0;
  String hdr, body;
};

static bool rtsp(WiFiClientSecure &c, const char *method, const String &uri, const String &extra, Resp &r) {
  g_cseq++;
  String req = String(method) + " " + uri + " RTSP/1.0\r\nCSeq: " + g_cseq + "\r\nUser-Agent: BambuDashboard\r\n" +
               auth_for(method, uri) + extra + "\r\n";
  c.print(req);
  r = Resp();
  uint32_t t0 = millis();
  while (!r.hdr.endsWith("\r\n\r\n")) {
    if (c.available() > 0) {
      int ch = c.read();
      if (r.hdr.length() == 0 && ch == '$') {  // trame RTP entrelacée arrivée avant la réponse : on la saute
        uint8_t h[3];
        if (!rd(c, h, 3, 2000)) return false;
        size_t len = ((size_t)h[1] << 8) | h[2];
        uint8_t tmp[64];
        while (len > 0) {
          size_t k = len < sizeof(tmp) ? len : sizeof(tmp);
          if (!rd(c, tmp, k, 2000)) return false;
          len -= k;
        }
        g_skipped++;
        continue;
      }
      r.hdr += (char)ch;
      if (r.hdr.length() > 4096) return false;
    } else {
      if (!c.connected() || millis() - t0 > 7000) return false;
      vTaskDelay(pdMS_TO_TICKS(3));
    }
  }
  int sp = r.hdr.indexOf(' ');
  r.code = sp > 0 ? r.hdr.substring(sp + 1, sp + 4).toInt() : 0;
  String hl = r.hdr;
  hl.toLowerCase();
  int ci = hl.indexOf("content-length:");
  if (ci >= 0) {
    int len = hl.substring(ci + 15, hl.indexOf('\r', ci)).toInt();
    if (len > 0 && len < 6000) {
      uint8_t tmp[64];
      int left = len;
      while (left > 0) {
        int k = left < (int)sizeof(tmp) ? left : (int)sizeof(tmp);
        if (!rd(c, tmp, k, 4000)) return false;
        r.body.concat((const char *)tmp, k);
        left -= k;
      }
    }
  }
  return true;
}

static const char *profile_name(int p) {
  switch (p) {
    case 66: return "Baseline";
    case 77: return "Main";
    case 88: return "Extended";
    case 100: return "High";
    case 110: return "High 10";
    case 122: return "High 4:2:2";
    case 244: return "High 4:4:4";
    default: return "autre";
  }
}

static void feed_nal_b64(const String &b64, bool is_sps) {
  unsigned char out[128];
  size_t ol = 0;
  if (mbedtls_base64_decode(out, sizeof(out), &ol, (const unsigned char *)b64.c_str(), b64.length()) != 0 || ol < 2) return;
  if (is_sps) parse_sps(out, ol);
  else parse_pps(out, ol);
  if (g_dec) g_dec->feed(out, ol);
  if (is_sps) {
    memcpy(g_cur_sps, out, ol);
    g_cur_sps_len = ol;
  } else {
    memcpy(g_cur_pps, out, ol);
    g_cur_pps_len = ol;
  }
}

static void run_probe(const PrinterCfg &cfg) {
  g_digest = g_basic = false;
  g_cseq = 0;
  g_sps = SpsInfo();
  g_cabac = -1;
  st_frames = st_bytes = st_idr = st_i = st_p = st_b = st_pkts = st_lost = 0;
  st_last_seq = -1;
  g_pass = cfg.code;
  g_pass.trim();

  WiFiClientSecure c;
  c.setInsecure();
  c.setHandshakeTimeout(8);
  c.setTimeout(5);
  rep("Connexion TLS a %s:%u ...", cfg.ip.c_str(), RTSP_PORT);
  uint32_t t0 = millis();
  if (!c.connect(cfg.ip.c_str(), RTSP_PORT)) {
    rep("ECHEC : connexion impossible. LAN Mode Liveview est-il active sur l'imprimante ?");
    return;
  }
  rep("TLS etabli en %u ms", (unsigned)(millis() - t0));

  String base = "rtsps://" + cfg.ip + ":" + RTSP_PORT + "/streaming/live/1";
  Resp r;
  if (!rtsp(c, "DESCRIBE", base, "Accept: application/sdp\r\n", r)) {
    rep("ECHEC : pas de reponse au DESCRIBE");
    return;
  }
  if (r.code == 401) {
    parse_auth(r.hdr);
    rep("Authentification demandee : %s", g_digest ? "Digest" : (g_basic ? "Basic" : "inconnue"));
    if (!rtsp(c, "DESCRIBE", base, "Accept: application/sdp\r\n", r)) {
      rep("ECHEC : pas de reponse au DESCRIBE authentifie");
      return;
    }
  }
  rep("DESCRIBE : code %d", r.code);
  if (r.code == 401 || r.code == 403) {
    rep("ECHEC : code d'acces refuse (verifier le code LAN)");
    return;
  }
  if (r.code != 200) {
    rep("ECHEC : reponse inattendue (%d)", r.code);
    return;
  }

  // SDP : piste video, contrôle, paramètres H.264
  String control, sprops;
  bool in_video = false;
  int pos = 0;
  while (pos < (int)r.body.length()) {
    int e = r.body.indexOf('\n', pos);
    if (e < 0) e = r.body.length();
    String line = r.body.substring(pos, e);
    pos = e + 1;
    line.trim();
    if (line.startsWith("m=")) in_video = line.startsWith("m=video");
    if (!in_video) continue;
    if (line.startsWith("a=control:")) control = line.substring(10);
    if (line.startsWith("a=rtpmap:")) rep("SDP : %s", line.c_str() + 9);
    if (line.startsWith("a=fmtp:")) {
      int i = line.indexOf("sprop-parameter-sets=");
      if (i >= 0) sprops = qv(line.substring(i), "sprop-parameter-sets=");
    }
  }
  if (control.length() == 0 && r.body.indexOf("m=video") < 0) {
    rep("ECHEC : pas de piste video dans le SDP");
    return;
  }
  DecGuard dec_guard;
  if (sprops.length()) {
    int comma = sprops.indexOf(',');
    feed_nal_b64(comma < 0 ? sprops : sprops.substring(0, comma), true);
    if (comma > 0) feed_nal_b64(sprops.substring(comma + 1), false);
  }

  String ctl = base;
  if (control.startsWith("rtsp")) ctl = control;
  else if (control.length() && control != "*") ctl = base + "/" + control;
  if (!rtsp(c, "SETUP", ctl, "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n", r) || r.code != 200) {
    rep("ECHEC : SETUP refuse (code %d)", r.code);
    return;
  }
  String sess;
  {
    String hl = r.hdr;
    hl.toLowerCase();
    int i = hl.indexOf("session:");
    if (i >= 0) {
      int e = r.hdr.indexOf('\r', i);
      sess = r.hdr.substring(i + 8, e);
      int sc = sess.indexOf(';');
      if (sc >= 0) sess = sess.substring(0, sc);
      sess.trim();
    }
  }
  rep_hdr("SETUP", r.hdr);
  g_skipped = 0;
  uint32_t t_play = millis();
  bool play_ok = rtsp(c, "PLAY", base, "Session: " + sess + "\r\n", r);
  rep("PLAY : %s, code %d, %u ms, connexion %s, %u trames sautees", play_ok ? "reponse lue" : "pas de reponse lisible", r.code,
      (unsigned)(millis() - t_play), c.connected() ? "ouverte" : "fermee", (unsigned)g_skipped);
  if (r.hdr.length()) rep_hdr("PLAY brut", r.hdr);
  if (r.code != 0 && r.code != 200) {
    rep("ECHEC : PLAY refuse (code %d)", r.code);
    return;
  }
  if (!c.connected() && c.available() <= 0) {
    rep("ECHEC : connexion fermee par l'imprimante apres PLAY");
    return;
  }
  rep("Lecture du flux sur %u s", (unsigned)(PLAY_MS / 1000));

  uint8_t *buf = (uint8_t *)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) {
    rep("ECHEC : memoire insuffisante");
    return;
  }
  uint32_t t_start = millis(), t_rx = t_start;
  g_last_pic_ms = t_start - VIEW_PERIOD_MS;
  g_want = false;
  if (g_view) {
    rep("En direct : une image par seconde au mieux");
    g_dec_quit = false;
    g_dec_done = false;
    g_job_state = 0;
    // cœur 0 (priorité 0, sous la lecture du flux) : sur le cœur 1, la boucle LVGL l'occupe presque tout le temps (décodage mesuré 2,7 s et 4,6 s)
    if (xTaskCreatePinnedToCore(dec_task, "cam_dec", 20480, nullptr, 0, nullptr, 0) != pdPASS) {
      g_dec_done = true;
      heap_caps_free(buf);
      rep("ECHEC : tache de decodage impossible a creer");
      return;
    }
  }
  uint32_t t_ka = t_start;
  while (g_view ? !g_stop : (millis() - t_start < PLAY_MS)) {
    if (g_view && millis() - t_ka > 5000) {
      // message de maintien de session (OPTIONS) ; la reponse, du texte sans '$', est ignoree par la boucle de lecture
      t_ka = millis();
      g_cseq++;
      c.print(String("OPTIONS ") + base + " RTSP/1.0\r\nCSeq: " + g_cseq + "\r\nUser-Agent: BambuDashboard\r\n" + auth_for("OPTIONS", base) +
              "Session: " + sess + "\r\n\r\n");
    }
    if (c.available() <= 0) {
      if (!c.connected()) break;
      if (g_view && millis() - t_rx > 10000) {
        rep("Flux interrompu : plus de donnees depuis 10 s (apres %u s, %u paquets, connexion %s)", (unsigned)((millis() - t_start) / 1000),
            (unsigned)st_pkts, c.connected() ? "ouverte" : "fermee");
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(2));
      continue;
    }
    if (c.read() != '$') continue;
    uint8_t h[3];
    if (!rd(c, h, 3, 2000)) break;
    size_t len = ((size_t)h[1] << 8) | h[2];
    if (len > 4096) {
      uint8_t tmp[64];
      while (len > 0) {
        size_t k = len < sizeof(tmp) ? len : sizeof(tmp);
        if (!rd(c, tmp, k, 2000)) break;
        len -= k;
      }
      continue;
    }
    if (!rd(c, buf, len, 2000)) break;
    t_rx = millis();
    if (h[0] == 0) handle_rtp(buf, len);
  }
  float secs = (millis() - t_start) / 1000.0f;
  if (g_view) {
    g_dec_quit = true;
    while (!g_dec_done) vTaskDelay(pdMS_TO_TICKS(20));
  }
  heap_caps_free(buf);
  rtsp(c, "TEARDOWN", base, "Session: " + sess + "\r\n", r);
  c.stop();

  if (st_pkts == 0) {
    rep("ECHEC : aucun paquet video recu");
    return;
  }
  if (g_view) {
    rep("Flux arrete : %u paquets, %u perdus, %u erreurs de decodage %s", (unsigned)st_pkts, (unsigned)st_lost, (unsigned)g_dec_errs,
        g_dec_err);
    return;
  }
  if (g_sps.ok)
    rep("Profil H.264 : %s (%d), niveau %d.%d, contrainte 0x%02X", profile_name(g_sps.profile), g_sps.profile, g_sps.level / 10,
        g_sps.level % 10, g_sps.constraint);
  else
    rep("Profil H.264 : SPS non trouve");
  if (g_sps.ok) rep("Resolution : %d x %d", g_sps.w, g_sps.h);
  rep("Codage entropique : %s", g_cabac < 0 ? "inconnu" : (g_cabac ? "CABAC" : "CAVLC"));
  rep("Recu en %.1f s : %u images (%.1f /s), %u Ko/s, %u paquets, %u perdus", secs, (unsigned)st_frames, st_frames / secs,
      (unsigned)(st_bytes / 1024.0f / secs), (unsigned)st_pkts, (unsigned)st_lost);
  rep("Tranches : I %u, P %u, B %u (IDR %u)", (unsigned)st_i, (unsigned)st_p, (unsigned)st_b, (unsigned)st_idr);
  if (g_pics > 0) {
    String ms;
    for (uint32_t i = 0; i < g_pics; i++) ms += String(i ? " / " : "") + String((unsigned)g_pic_ms[i]);
    rep("Decodeur maison (High, CABAC) : %u images IDR, %s ms chacune, %dx%d", (unsigned)g_pics, ms.c_str(), g_dec->width(),
        g_dec->height());
  } else {
    rep("Decodeur maison : aucune image complete (%u erreurs) %s", (unsigned)g_dec_errs, g_dec_err);
  }
  rep("PSRAM libre : %u Mo", (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / (1024 * 1024)));

  bool cbp = g_sps.ok && g_sps.profile == 66;
  bool simple = g_sps.ok && g_cabac == 0 && st_b == 0 && g_sps.profile <= 77;
  if (cbp && g_cabac != 1 && st_b == 0) rep("VERDICT : compatible avec le decodeur esp_h264");
  else if (simple) rep("VERDICT : a verifier (profil Main sans CABAC ni image B)");
  else rep("VERDICT : INCOMPATIBLE avec esp_h264 (profil/CABAC/images B)");
}

// ---------------------------------------------------------------- tâche
static void probe_task(void *arg) {
  PrinterCfg *p = (PrinterCfg *)arg;
  PrinterCfg cfg = *p;
  delete p;
  rep("Imprimante : %s", cfg.name.c_str());
  bool got = xSemaphoreTake(tls_gate(), pdMS_TO_TICKS(20000)) == pdTRUE;
  if (!got) {
    rep("ECHEC : reseau occupe, reessayer");
  } else {
    bambu_pause(true);
    rep("Memoire interne : bloc max %u", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    run_probe(cfg);
    rep("Memoire interne apres : bloc max %u", (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
    bambu_pause(false);
    xSemaphoreGive(tls_gate());
  }
  rep("Fin de la sonde.");
  g_running = false;
  vTaskDelete(nullptr);
}

static void start_task(const PrinterCfg &cfg);

bool camera_view_frame(const uint16_t **pix, int *w, int *h, uint32_t *seq) {
  if (!g_vbuf[0] || g_vseq == 0) return false;
  int f = g_front;
  *pix = g_vbuf[f];
  *w = g_vw;
  *h = g_vh;
  *seq = g_vseq;
  return true;
}

uint32_t camera_view_decode_ms() { return g_vms; }

String camera_view_status() {
  if (!g_mx) return "";
  xSemaphoreTake(g_mx, portMAX_DELAY);
  String r = g_last_line;
  xSemaphoreGive(g_mx);
  return r;
}

void camera_view_stop() { g_stop = true; }

bool camera_view_start(const PrinterCfg &cfg) {
  if (g_running) return false;
  if (!g_vbuf[0]) {
    for (int i = 0; i < 2; i++) g_vbuf[i] = (uint16_t *)heap_caps_malloc((size_t)VIEW_MAXW * VIEW_MAXH * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_vbuf[0] || !g_vbuf[1]) {
      g_vbuf[0] = g_vbuf[1] = nullptr;  // fuite negligeable : ne se produit qu'en cas de manque de PSRAM
      return false;
    }
  }
  g_vseq = 0;
  g_vw = g_vh = 0;
  g_vms = 0;
  g_view = true;
  g_stop = false;
  start_task(cfg);
  return true;
}

static void start_task(const PrinterCfg &cfg) {
  if (g_running) return;
  if (!g_mx) g_mx = xSemaphoreCreateMutex();
  xSemaphoreTake(g_mx, portMAX_DELAY);
  g_report = "";
  g_last_line = "Demarrage...";
  xSemaphoreGive(g_mx);
  g_running = true;
  xTaskCreatePinnedToCore(probe_task, "cam_probe", 20480, new PrinterCfg(cfg), g_view ? 1 : 0, nullptr, 0);
}

void camera_probe_start(const PrinterCfg &cfg) {
  g_view = false;
  start_task(cfg);
}
