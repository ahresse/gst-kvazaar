/* GStreamer HEVC encoder plugin
 * Copyright (C) <2019> Alexandre Esse <alexandre.esse.dev@gmail.com>
 *
 * This file is part of gst-kvazaar.
 *
 * gst-kvazaar is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * gst-kvazaar is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with gst-kvazaar.  If not, see <https://www.gnu.org/licenses/>.
 */

//#define HAS_CRYPTO
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef __GST_KVAZAAR_ENC_H__
#define __GST_KVAZAAR_ENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>
#include <kvazaar.h>

G_BEGIN_DECLS
#define GST_TYPE_KVAZAAR_ENC \
  (gst_kvazaar_enc_get_type())
#define GST_KVAZAAR_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_KVAZAAR_ENC,GstKvazaarEnc))
#define GST_KVAZAAR_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_KVAZAAR_ENC,GstKvazaarEncClass))
#define GST_IS_KVAZAAR_ENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_KVAZAAR_ENC))
#define GST_IS_KVAZAAR_ENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_KVAZAAR_ENC))
typedef struct _GstKvazaarEnc GstKvazaarEnc;
typedef struct _GstKvazaarEncClass GstKvazaarEncClass;

struct _GstKvazaarEnc
{
  GstVideoEncoder element;

  /*< private > */
  kvz_encoder *kvazaarenc;
  kvz_config *kvazaarconfig;
  GstClockTime dts_offset;
  gboolean push_header;
  const kvz_api *api;
  guint32 systeme_frame_number_offset;

  /* List of frame/buffer mapping structs for
   * pending frames */
  GList *pending_frames;

  /* properties */
  guint    bitrate;          /* Bitrate */
  gint     qp;               /* Quantization parameter */
  gint     intra_period;     /* The period of intra frames in stream */
  gint     vps_period;       /* How often the VPS, SPS and PPS are re-sent */
  gboolean no_psnr;          /* Print PSNR in CLI ? */
  gboolean no_info;          /* Add SEI info ? */
  gint     preset;           /* Use preset */
#ifdef HAS_CRYPTO
  gint     crypto;           /* Crypto mode */
  GString  *key;             /* Crypto key as a string */
#endif
  guint    source_scan_type; /* Source scan type
                 (0: progressive, 1: top field first, 2: bottom field first). */
  guint    aud_enable;       /* Flag to use access unit delimiters */
  gint     ref_frames;       /* Number of reference frames to use */
  GString  *pu_depth_intra;  /*  */
  GString  *pu_depth_inter;  /*  */
  guint    rdo;              /* RD-calculation level (0..2) */
  guint    me;               /* Integer motion estimation algorithm */
  GString  *deblock;         /* Deblocking filter <beta:tc> */
  gboolean signhide;         /* Enable sign hiding */
  gint     subme;            /* Fractional pixel motion estimation level */
  gint     sao;              /* Flag to enable sample adaptive offset filter */
  gboolean rdoq;             /* Enable Rate-Distortion Optimized Quantization */
  gboolean rdoq_skip;        /* Skips RDOQ for 4x4 blocks */
  gboolean trskip;           /* Enable transform skip (for 4x4 blocks) */
  gboolean full_intra_search;/* Try all intra modes during rough search. */
  gboolean mv_rdo;           /* Rate-Distortion Optimized motion vector costs */
  gboolean smp;              /* Symmetric Motion Partition */
  gboolean amp;              /* Asymmetric Motion Partition */
  gint cu_split_termination; /* CU split search termination condition */
  gint me_early_termination; /* ME early termination condition */
  GString  *gop;             /* String that defines a GOP structure */
  GString  *roi;             /* Name of the file describing a ROI */
  GString  *kvz_opts;       /* Options string to pass to Kvazaar config_parse */
  /*gint input_fps;*/
  /*GString *input_res;*/
  /*GString *input_format;*/

  gint     *deblock_beta;    /* (deblocking) beta offset (div 2), range -6...6*/
  gint     *deblock_tc;      /* (deblocking) tc offset (div 2), range -6...6 */

  gint     roi_width;        /* (deblocking) tc offset (div 2), range -6...6 */
  gint     roi_height;       /* (deblocking) tc offset (div 2), range -6...6 */
  int8_t   *dqps;            /* (deblocking) tc offset (div 2), range -6...6 */

  /* Used to not overwrite preset */
  gboolean deblock_set;           /* true if deblock has been set by user */
  gboolean signhide_set;          /* true if signhide has been set by user */
  gboolean rdoq_set;              /* true if rdoq has been set by user */
  gboolean rdoq_skip_set;         /* true if rdoq_skip has been set by user */
  gboolean trskip_set;            /* true if trskip has been set by user */
  gboolean full_intra_search_set; /* true if trskip has been set by user */
  gboolean mv_rdo_set;            /* true if mv_rdo has been set by user */
  gboolean smp_set;               /* true if smp has been set by user */
  gboolean amp_set;               /* true if amp has been set by user */
  gboolean gop_set;               /* true if gop has been set by user */
  gboolean roi_set;               /* true if roi has been set by user */

  /* input description */
  GstVideoCodecState *input_state;

  /* configuration changed  while playing */
  gboolean reconfig;

  /* from the downstream caps */
  const gchar *peer_profile;
  gboolean peer_intra_profile;

};

struct _GstKvazaarEncClass
{
  GstVideoEncoderClass parent_class;
};

GType gst_kvazaar_enc_get_type (void);

G_END_DECLS
#endif /* __GST_KVAZAAR_ENC_H__ */
