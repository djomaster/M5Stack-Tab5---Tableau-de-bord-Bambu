#pragma once
// Decodeur H.264 "intra seulement" : profils Baseline/Main/High, 4:2:0 8 bits, CABAC, images I/IDR uniquement.
// Ecrit pour les flux des imprimantes Bambu (High 4.1, CABAC) que le decodeur esp_h264 ne sait pas lire.
// Code portable (aucune dependance Arduino) : il se verifie sur un PC contre ffmpeg (tools/h264i_test.cpp).
// Limites : pas d'images P/B, pas de CAVLC, pas de matrices de quantification, pas d'entrelace, pas d'I_PCM,
// pas de filtre de deblocage (voir docs/DECISIONS.md, D-026).
#include <stddef.h>
#include <stdint.h>

typedef void *(*h264i_alloc_fn)(size_t);
typedef void (*h264i_free_fn)(void *);

struct H264IMb;

class H264Intra {
 public:
  explicit H264Intra(h264i_alloc_fn a = nullptr, h264i_free_fn f = nullptr);
  ~H264Intra();

  // Une unite NAL sans code de debut. 0 : traitee ; 1 : ignoree ; <0 : erreur (voir last_error()).
  int feed(const uint8_t *nal, size_t len);

  bool picture_ready() const { return mbs_done_ >= mb_total_ && mb_total_ > 0; }
  int width() const { return crop_w_; }    // dimensions affichables (apres recadrage)
  int height() const { return crop_h_; }
  int coded_width() const { return mbw_ * 16; }
  int coded_height() const { return mbh_ * 16; }
  const uint8_t *plane(int i) const { return pl_[i]; }
  int stride(int i) const { return i == 0 ? mbw_ * 16 : mbw_ * 8; }
  const char *last_error() const { return err_; }
  int mbs_done() const { return mbs_done_; }

 private:
  struct Sps {
    bool ok = false;
    int profile = 0, chroma = 1, log2_fn = 4, poc_type = 0, log2_poc = 4, mbw = 0, mbh = 0;
    bool delta_zero = false, frame_mbs_only = true, scaling = false;
    int crop_l = 0, crop_r = 0, crop_t = 0, crop_b = 0;
  };
  struct Pps {
    bool ok = false;
    int sps_id = 0;
    bool cabac = false, poc_present = false, deblock_ctrl = false, constrained = false, redundant = false, t8 = false, scaling = false;
    int slice_groups = 0, init_qp = 26, cqp_off = 0, cqp_off2 = 0;
  };

  void *alloc(size_t n);
  void release(void *p);
  bool setup_picture();
  int parse_sps(const uint8_t *rbsp, size_t n);
  int parse_pps(const uint8_t *rbsp, size_t n);
  int decode_slice(const uint8_t *rbsp, size_t n);
  bool fail(const char *m);

  h264i_alloc_fn alloc_;
  h264i_free_fn free_;
  Sps sps_;
  Pps pps_;
  int mbw_ = 0, mbh_ = 0, crop_w_ = 0, crop_h_ = 0;
  int mb_total_ = 0, mbs_done_ = 0;
  uint8_t *pl_[3] = {nullptr, nullptr, nullptr};
  H264IMb *mb_ = nullptr;
  uint8_t *rbsp_ = nullptr;
  size_t rbsp_cap_ = 0;
  const char *err_ = "";
  int slice_no_ = 0;
  friend struct H264ISlice;
};
