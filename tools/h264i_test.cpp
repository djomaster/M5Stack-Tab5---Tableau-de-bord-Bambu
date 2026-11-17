// Banc d'essai sur PC : decode la premiere image IDR d'un flux Annex B et ecrit le YUV 4:2:0 recadre.
// Usage : h264i_test flux.h264 sortie.yuv [numero_image_IDR]
#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "h264i.h"

int main(int argc, char **argv) {
  if (argc < 3) return 2;
  FILE *f = fopen(argv[1], "rb");
  if (!f) return 2;
  std::vector<uint8_t> d;
  uint8_t buf[65536];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.insert(d.end(), buf, buf + n);
  fclose(f);
  int want = argc > 3 ? atoi(argv[3]) : 0, idr_seen = -1;
  // decoupe en NAL
  std::vector<std::pair<size_t, size_t>> nals;
  for (size_t i = 0; i + 3 < d.size();) {
    if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
      size_t s = i + 3, e = s;
      while (e + 2 < d.size() && !(d[e] == 0 && d[e + 1] == 0 && (d[e + 2] == 1 || d[e + 2] == 0))) e++;
      if (e + 2 >= d.size()) e = d.size();
      nals.push_back({s, e - s});
      i = e;
    } else i++;
  }
  H264Intra dec;
  double ms = 0;
  bool done = false;
  for (auto &p : nals) {
    const uint8_t *nal = &d[p.first];
    int t = nal[0] & 0x1F;
    if (t == 5) {
      // nouvelle image IDR : premiere tranche a first_mb = 0 (bit de tete a 1)
      if (nal[1] & 0x80) idr_seen++;
      if (idr_seen != want) continue;
    } else if (t != 7 && t != 8) continue;
    auto t0 = std::chrono::steady_clock::now();
    int r = dec.feed(nal, p.second);
    ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (r < 0) {
      fprintf(stderr, "erreur : %s (mb %d)\n", dec.last_error(), dec.mbs_done());
      return 1;
    }
    if (t == 5 && dec.picture_ready()) {
      done = true;
      break;
    }
  }
  if (!done) {
    fprintf(stderr, "image incomplete (%d MB)\n", dec.mbs_done());
    return 1;
  }
  int w = dec.width(), h = dec.height();
  FILE *o = fopen(argv[2], "wb");
  for (int i = 0; i < 3; i++) {
    int pw = i ? w / 2 : w, ph = i ? h / 2 : h;
    for (int y = 0; y < ph; y++) fwrite(dec.plane(i) + y * dec.stride(i), 1, pw, o);
  }
  fclose(o);
  fprintf(stderr, "ok %dx%d en %.1f ms (PC)\n", w, h, ms);
  return 0;
}
