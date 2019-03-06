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

/**
 * SECTION:element-kvazaarenc
 * @title: kvazaarenc
 *
 * This element encodes raw video into HEVC/H.265 compressed data.
 *
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstkvazaarenc.h"

#include <gst/pbutils/pbutils.h>
#include <gst/video/video.h>
#include <gst/video/gstvideometa.h>
#include <gst/video/gstvideopool.h>

#include <string.h>
#include <stdlib.h>

GST_DEBUG_CATEGORY_STATIC (kvazaar_enc_debug);
#define GST_CAT_DEFAULT kvazaar_enc_debug

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
#define FORMATS "I420, I420_10LE"
#else
#define FORMATS "I420, I420_10BE"
#endif

enum
{
  PROP_0,
  PROP_BITRATE,
  PROP_QP,
  PROP_INTRA_PERIOD,
  PROP_VPS_PERIOD,
  PROP_NO_PSNR,
  PROP_NO_INFO,
  PROP_PRESET,
#ifdef HAS_CRYPTO
  PROP_CRYPTO,
  PROP_KEY,
#endif
  PROP_SOURCE_SCAN_TYPE,
  PROP_AUD,
  PROP_REF_FRAMES,
  PROP_PU_DEPTH_INTRA,
  PROP_PU_DEPTH_INTER,
  PROP_RDO,
  PROP_ME,
  PROP_DEBLOCK,
  PROP_SIGNHIDE,
  PROP_SUBME,
  PROP_SAO,
  PROP_RDOQ,
  PROP_RDOQ_SKIP,
  PROP_TRSKIP,
  PROP_FULL_INTRA_SEARCH,
  PROP_MV_RDO,
  PROP_SMP,
  PROP_AMP,
  PROP_CU_SPLIT_TERM,
  PROP_ME_EARLY_TERM,
  PROP_GOP,
  PROP_ROI,
  PROP_KVZ_OPTS
};

typedef enum {
  GST_KVAZAAR_ENC_NO_PRESET,
  GST_KVAZAAR_ENC_ULTRAFAST,
  GST_KVAZAAR_ENC_SUPERFAST,
  GST_KVAZAAR_ENC_VERYFAST,
  GST_KVAZAAR_ENC_FASTER,
  GST_KVAZAAR_ENC_FAST,
  GST_KVAZAAR_ENC_MEDIUM,
  GST_KVAZAAR_ENC_SLOW,
  GST_KVAZAAR_ENC_SLOWER,
  GST_KVAZAAR_ENC_VERYSLOW,
  GST_KVAZAAR_ENC_PLACEBO
} GstKvazaarencPreset;

static const GEnumValue preset_types[] = {
  { GST_KVAZAAR_ENC_NO_PRESET, "No preset", "none" },
  { GST_KVAZAAR_ENC_ULTRAFAST, "Ultrafast", "ultrafast" },
  { GST_KVAZAAR_ENC_SUPERFAST, "Superfast", "superfast" },
  { GST_KVAZAAR_ENC_VERYFAST,  "Veryfast",  "veryfast" },
  { GST_KVAZAAR_ENC_FASTER,    "Faster",    "faster" },
  { GST_KVAZAAR_ENC_FAST,      "Fast",      "fast" },
  { GST_KVAZAAR_ENC_MEDIUM,    "Medium",    "medium" },
  { GST_KVAZAAR_ENC_SLOW,      "Slow",      "slow" },
  { GST_KVAZAAR_ENC_SLOWER,    "Slower",    "slower" },
  { GST_KVAZAAR_ENC_VERYSLOW,  "Veryslow",  "veryslow" },
  { GST_KVAZAAR_ENC_PLACEBO,   "Placebo",   "placebo" },
  { 0, NULL, NULL },
};

typedef enum {
  GST_KVAZAAR_PROGRESSIVE,
  GST_KVAZAAR_TFF,
  GST_KVAZAAR_BFF,
} GstKvazaarencSourceScanType;

typedef enum {
  GST_KVAZAAR_ENC_RDO_SKIP,
  GST_KVAZAAR_ENC_RDO_SATD,
  GST_KVAZAAR_ENC_RDO_SSE,
  GST_KVAZAAR_ENC_RDO_DEFAULT
} GstKvazaarencRdo;

typedef enum {
  GST_KVAZAAER_ENC_SUBME_0,
  GST_KVAZAAER_ENC_SUBME_1,
  GST_KVAZAAER_ENC_SUBME_2,
  GST_KVAZAAER_ENC_SUBME_3,
  GST_KVAZAAER_ENC_SUBME_4,
  GST_KVAZAAER_ENC_SUBME_DEFAULT
} GstKvazaarencSubme;

static GEnumValue subme_types[] = {
  { GST_KVAZAAER_ENC_SUBME_0, "only integer motion estimation",      "0" },
  { GST_KVAZAAER_ENC_SUBME_1, "+ 1/2-pixel horizontal and vertical", "1" },
  { GST_KVAZAAER_ENC_SUBME_2, "+ 1/2-pixel diagonal",                "2" },
  { GST_KVAZAAER_ENC_SUBME_3, "+ 1/4-pixel horizontal and vertical", "3" },
  { GST_KVAZAAER_ENC_SUBME_4, "+ 1/4-pixel diagonal",                "4" },
  { GST_KVAZAAER_ENC_SUBME_DEFAULT, "Use Kvazaar default (4) or preset", "default" },
  { 0, NULL, NULL },
};

typedef enum {
  GST_KVAZAAER_ENC_SAO_OFF,
  GST_KVAZAAER_ENC_SAO_EDGE,
  GST_KVAZAAER_ENC_SAO_BAND,
  GST_KVAZAAER_ENC_SAO_FULL,
  GST_KVAZAAER_ENC_SAO_DEFAULT
} GstKvazaarencSao;

static GEnumValue sao_types[] = {
  { GST_KVAZAAER_ENC_SAO_OFF,  "Disable sample adaptive offset filter", "off" },
  { GST_KVAZAAER_ENC_SAO_EDGE, "Edge",                                 "edge" },
  { GST_KVAZAAER_ENC_SAO_BAND, "Band",                                 "band" },
  { GST_KVAZAAER_ENC_SAO_FULL, "Full", "full" },
  { GST_KVAZAAER_ENC_SAO_DEFAULT, "Use Kvazaar default (full) or preset", "default" },
  { 0, NULL, NULL },
};

#define PROP_BITRATE_DEFAULT		    0
#define PROP_QP_DEFAULT             32
#define PROP_INTRA_PERIOD_DEFAULT   0
#define PROP_VPS_PERIOD_DEFAULT     0
#define PROP_PRESET_DEFAULT         GST_KVAZAAR_ENC_NO_PRESET
#define PROP_KEY_DEFAULT "16,213,27,56,255,127,242,112,97,126,197,204,25,59,38,30"
#define PROP_REF_FRAMES_DEFAULT     0
#define PROP_PU_DEPTH_INTRA_DEFAULT ""
#define PROP_PU_DEPTH_INTER_DEFAULT ""
#define PROP_RDO_DEFAULT            GST_KVAZAAR_ENC_RDO_DEFAULT
#define PROP_ME_DEFAULT             31
#define PROP_DEBLOCK_DEFAULT        "true"
#define PROP_CU_SPLIT_TERM_DEFAULT  -1
#define PROP_ME_EARLY_TERM_DEFAULT  -1
#define PROP_GOP_DEFAULT            "lp-g4d3t1"

#define KVAZAAR_PARAM_BAD_NAME  (-1)
#define KVAZAAR_PARAM_BAD_VALUE (-2)

#define GST_KVAZAAR_ENC_PRESET_TYPE (gst_kvazaar_enc_preset_get_type())
static GType
gst_kvazaar_enc_preset_get_type (void)
{
  static GType kvazaarenc_preset_type = 0;

  if (!kvazaarenc_preset_type) {
    kvazaarenc_preset_type =
      g_enum_register_static ("GstKvazaarencPreset", preset_types);
  }

  return kvazaarenc_preset_type;
}

#ifdef HAS_CRYPTO
#define GST_KVAZAAR_ENC_CRYPTO_TYPE (gst_kvazaar_enc_crypto_get_type())
static GType
gst_kvazaar_enc_crypto_get_type (void)
{
  static GType kvazaarenc_crypto_type = 0;

  if (!kvazaarenc_crypto_type) {
    static GFlagsValue crypto_types[] = {
      { KVZ_CRYPTO_OFF,                "Off - disable",         "off" },
      { KVZ_CRYPTO_MVs,                "MVs",                   "mvs" },
      { KVZ_CRYPTO_MV_SIGNS,           "MV signs",              "mv-signs" },
      { KVZ_CRYPTO_TRANSF_COEFFS,      "Transf coeffs",         "transf-coeffs" },
      { KVZ_CRYPTO_TRANSF_COEFF_SIGNS, "Transform coeff signs", "transf-coeff-signs" },
      { KVZ_CRYPTO_INTRA_MODE,         "Intra prediction mode", "intra-mode" },
      { KVZ_CRYPTO_ON,                 "On - every mode",       "on" },
      { 0, NULL, NULL },
    };

    kvazaarenc_crypto_type =
    g_flags_register_static ("GstKvazaarencCrypto", crypto_types);
  }

  return kvazaarenc_crypto_type;
}
#endif

#define GST_KVAZAAR_ENC_SOURCE_SCAN_TYPE_TYPE (gst_kvazaar_enc_source_scan_type_get_type())
static GType
gst_kvazaar_enc_source_scan_type_get_type (void)
{
  static GType kvazaarenc_source_scan_type_type = 0;

  if (!kvazaarenc_source_scan_type_type) {
    static GEnumValue source_scan_type_types[] = {
      { GST_KVAZAAR_PROGRESSIVE, "Progressive",        "progressive" },
      { GST_KVAZAAR_TFF,         "Top Field First",    "tff" },
      { GST_KVAZAAR_BFF,         "Bottom Field First", "bff" },
      { 0, NULL, NULL },
    };

    kvazaarenc_source_scan_type_type =
      g_enum_register_static ("GstKvazaarencSourceScanType",
          source_scan_type_types);
  }

  return kvazaarenc_source_scan_type_type;
}

#define GST_KVAZAAR_ENC_RDO_TYPE (gst_kvazaar_enc_rdo_get_type())
static GType
gst_kvazaar_enc_rdo_get_type (void)
{
  static GType kvazaarenc_rdo_type = 0;

  if (!kvazaarenc_rdo_type) {
    static GEnumValue rdo_types[] = {
      { GST_KVAZAAR_ENC_RDO_SKIP,    "Skip intra if inter is good enough",   "skip" },
      { GST_KVAZAAR_ENC_RDO_SATD,    "Rough intra mode search with SATD",    "satd" },
      { GST_KVAZAAR_ENC_RDO_SSE,     "Refine intra mode search with SSE",    "sse" },
      { GST_KVAZAAR_ENC_RDO_DEFAULT, "Use Kvazaar default (satd) or preset", "default" },
      { 0, NULL, NULL },
    };

    kvazaarenc_rdo_type =
      g_enum_register_static ("GstKvazaarencRdo",
          rdo_types);
  }

  return kvazaarenc_rdo_type;
}

#define GST_KVAZAAR_ENC_ME_TYPE (gst_kvazaar_enc_me_get_type())
static GType
gst_kvazaar_enc_me_get_type (void)
{
  static GType kvazaarenc_me_type = 0;

  if (!kvazaarenc_me_type) {
    static GEnumValue me_types[] = {
      { KVZ_IME_HEXBS,  "HEXBS",   "hexbs" },
      { KVZ_IME_TZ,     "TZ",      "tz" },
      { KVZ_IME_FULL,   "Full",    "full" },
      { KVZ_IME_FULL8,  "Full 8",  "full8" },
      { KVZ_IME_FULL16, "Full 16", "full16" },
      { KVZ_IME_FULL32, "Full 32", "full32" },
      { KVZ_IME_FULL64, "Full 64", "full64" },
#ifdef KVZ_IME_DIA
      { KVZ_IME_DIA,    "DIA",     "dia" },
#endif
      { PROP_ME_DEFAULT, "Use Kvazaar default (hexbs) or preset", "default" },
      { 0, NULL, NULL },
    };

    kvazaarenc_me_type =
      g_enum_register_static ("GstKvazaarencMe",
          me_types);
  }

  return kvazaarenc_me_type;
}

#define GST_KVAZAAR_ENC_SUBME_TYPE (gst_kvazaar_enc_subme_get_type())
static GType
gst_kvazaar_enc_subme_get_type (void)
{
  static GType kvazaarenc_subme_type = 0;

  if (!kvazaarenc_subme_type) {
    kvazaarenc_subme_type =
      g_enum_register_static ("GstKvazaarencSubme", subme_types);
  }

  return kvazaarenc_subme_type;
}

#define GST_KVAZAAR_ENC_SAO_TYPE (gst_kvazaar_enc_sao_get_type())
static GType
gst_kvazaar_enc_sao_get_type (void)
{
  static GType kvazaarenc_sao_type = 0;

  if (!kvazaarenc_sao_type) {
    kvazaarenc_sao_type =
      g_enum_register_static ("GstKvazaarencSao", sao_types);
  }

  return kvazaarenc_sao_type;
}

#define GST_KVAZAAR_CU_SPLIT_TERM_TYPE (gst_kvazaar_enc_cu_split_term_get_type())
static GType
gst_kvazaar_enc_cu_split_term_get_type (void)
{
  static GType kvazaarenc_cu_split_term_type = 0;

  if (!kvazaarenc_cu_split_term_type) {
    static GEnumValue cu_split_term_types[] = {
      { KVZ_CU_SPLIT_TERMINATION_ZERO, "Terminate with zero residual", "zero" },
      { KVZ_CU_SPLIT_TERMINATION_OFF, "Never terminate cu-split search", "off" },
      { PROP_CU_SPLIT_TERM_DEFAULT, "Use Kvazaar default (zero) or preset", "default" },
      { 0, NULL, NULL },
    };

    kvazaarenc_cu_split_term_type =
      g_enum_register_static ("GstKvazaarencCuSplitTerm",
          cu_split_term_types);
  }

  return kvazaarenc_cu_split_term_type;
}

#define GST_KVAZAAR_ME_EARLY_TERM_TYPE (gst_kvazaar_enc_me_early_term_get_type())
static GType
gst_kvazaar_enc_me_early_term_get_type (void)
{
  static GType kvazaarenc_me_early_term_type = 0;

  if (!kvazaarenc_me_early_term_type) {
    static GEnumValue me_early_term_types[] = {
      { KVZ_ME_EARLY_TERMINATION_OFF, "Terminate with zero residual", "off" },
      { KVZ_ME_EARLY_TERMINATION_ON,  "Terminate early", "on" },
      { KVZ_ME_EARLY_TERMINATION_SENSITIVE, "Terminate even earlier", "sensitive" },
      { PROP_ME_EARLY_TERM_DEFAULT, "Use Kvazaar default (on) or preset", "default" },
      { 0, NULL, NULL },
    };

    kvazaarenc_me_early_term_type =
      g_enum_register_static ("GstKvazaarencMeEarlyTerm",
          me_early_term_types);
  }

  return kvazaarenc_me_early_term_type;
}

static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-raw, "
        "format = (string) { " FORMATS " }, "
        "framerate = (fraction) [0, MAX], "
        "width = (int) [ 4, MAX ], " "height = (int) [ 4, MAX ]")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/x-h265, "
        "framerate = (fraction) [0/1, MAX], "
        "width = (int) [ 4, MAX ], " "height = (int) [ 4, MAX ], "
        "stream-format = (string) byte-stream, "
        "alignment = (string) au, " "profile = (string) { main }")
    );

static void gst_kvazaar_enc_finalize (GObject * object);
static gboolean gst_kvazaar_enc_flush (GstVideoEncoder * encoder);

static GstFlowReturn gst_kvazaar_enc_finish (GstVideoEncoder * encoder);
static GstFlowReturn gst_kvazaar_enc_handle_frame (GstVideoEncoder * encoder,
    GstVideoCodecFrame * frame);
static void gst_kvazaar_enc_flush_frames (GstKvazaarEnc * encoder, gboolean send);
static GstFlowReturn gst_kvazaar_enc_encode_frame (GstKvazaarEnc * encoder, kvz_picture * cur_in_img,
    GstVideoCodecFrame * input_frame, uint32_t * len_out, gboolean send);

static gboolean gst_kvazaar_enc_set_format (GstVideoEncoder * video_enc,
    GstVideoCodecState * state);
static gboolean gst_kvazaar_enc_propose_allocation (GstVideoEncoder * encoder,
    GstQuery * query);
static gboolean gst_kvazaar_enc_init_encoder (GstKvazaarEnc * encoder);
static void gst_kvazaar_enc_close_encoder (GstKvazaarEnc * encoder);

static gboolean gst_kvazaar_enc_start (GstVideoEncoder * encoder);
static gboolean gst_kvazaar_enc_stop (GstVideoEncoder * encoder);

static void gst_kvazaar_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_kvazaar_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

#define gst_kvazaar_enc_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstKvazaarEnc, gst_kvazaar_enc, GST_TYPE_VIDEO_ENCODER,
    G_IMPLEMENT_INTERFACE (GST_TYPE_PRESET, NULL));

/*
 * Parse a string to an 8 bits integer within min and max.
 */
static int
parse_int8(const char *numstr, int8_t* number, int min, int max)
{
  char *tail;
  int d = strtol(numstr, &tail, 10);
  if (*tail || d < min || d > max)
  {
    GST_ERROR ("Expected number between %d and %d", min, max);
    if (number)
      *number = 0;
    return 0;
  }
  else
  {
    if (number)
      *number = d;
    return 1;
  }
}

/*
 * Parse a string to an integer within min and max.
 */
static int
parse_int (const char *numstr, int* number, int min, int max)
{
  char *tail;
  int d = strtol(numstr, &tail, 10);
  if (*tail || d < min || d > max)
  {
    GST_ERROR ("Expected number between %d and %d", min, max);
    if (number)
      *number = 0;
    return 0;
  }
  else
  {
    if (number)
      *number = d;
    return 1;
  }
}

/*
 * Parse a string to an array of integers within min and max.
 * The element delimiter can be ",", ";", ":" or " ".
 */
static int
parse_roi_array (const char *array, int *out_width, int *out_height,
    int8_t **out_dqps, int min, int max)
{
  char *key = g_strdup (array);
  const char delim[] = ",;: ";
  char *token;
  int i = 0;
  int width, height, size;
  int8_t* coeff_dqps;

  token = strtok (key, delim);
  if (token != NULL && !parse_int (token, &width, min, max))
  {
    free(key);
    return 0;
  }
  token = strtok (NULL, delim);
  if (token != NULL && !parse_int (token, &height, min, max))
  {
    free(key);
    return 0;
  }

  /* Now we have width and height, allocate dqps map */
  size = width * height;
  coeff_dqps = (int8_t *) g_malloc (sizeof (int8_t) * size);

  token = strtok (NULL, delim);
  while (token!=NULL)
  {
    if (!parse_int8 (token, &coeff_dqps[i], min, max))
    {
      free (key);
      free (coeff_dqps);
      return 0;
    }
    i++;
    token = strtok (NULL, delim);
  }
  if (i < size)
  {
    GST_ERROR ("parsing roi failed: too few delta QP.");
    g_free (key);
    free (coeff_dqps);
    return 0;
  }
  else if (i > size)
  {
    GST_ERROR ("parsing roi failed: too many delta QP.");
    g_free (key);
    free (coeff_dqps);
    return 0;
  }
  *out_width = width;
  *out_height = height;
  *out_dqps = coeff_dqps;
  g_free (key);
  return 1;
}

/*
 * Send options to the Kvazaar encoder.
 *
 * An option must be of form "<name>=<value>". Options must be seperated by
 * coma.
 *
 * returns 0 on success, 1 on failure.
 */
static int
parse_kvazaar_options (GstKvazaarEnc * encoder, char * array)
{
  char *opts_string = g_strdup (array);
  const char delim[] = ",";
  char *token;

  token = strtok (opts_string, delim);
  while (token != NULL)
  {
    char *name;
    char *value;

    value = g_strdup (token);
    name = strsep (&value, "=");

    if (!encoder->api->config_parse (encoder->kvazaarconfig, name, value))
    {
      GST_ERROR ("Error parsing option '%s' with value '%s'", name, value);
      return 1;
    }

    token = strtok (NULL, delim);
  }

  return 0;
}

static void
set_value (GValue * val, gint count, ...)
{
  const gchar *fmt = NULL;
  GValue sval = G_VALUE_INIT;
  va_list ap;
  gint i;

  g_value_init (&sval, G_TYPE_STRING);

  if (count > 1)
    g_value_init (val, GST_TYPE_LIST);

  va_start (ap, count);
  for (i = 0; i < count; i++) {
    fmt = va_arg (ap, const gchar *);
    g_value_set_string (&sval, fmt);
    if (count > 1) {
      gst_value_list_append_value (val, &sval);
    }
  }
  va_end (ap);

  if (count == 1)
    *val = sval;
  else
    g_value_unset (&sval);
}

static void
gst_kvazaar_enc_add_kvazaar_chroma_format (GstStructure * s,
    int kvazaar_chroma_format_local)
{
  GValue fmt = G_VALUE_INIT;

  if (KVZ_BIT_DEPTH >= 10) {
    GST_INFO ("This Kvazaar build supports %d-bit depth", KVZ_BIT_DEPTH);
    if (kvazaar_chroma_format_local == 0) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      set_value (&fmt, 4, "I420", "I420_10LE");
#else
      set_value (&fmt, 4, "I420", "I420_10BE");
#endif
    } else if (kvazaar_chroma_format_local == KVZ_CSP_420) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
      set_value (&fmt, 2, "I420", "I420_10LE");
#else
      set_value (&fmt, 2, "I420", "I420_10BE");
#endif
    } else {
      GST_ERROR ("Unsupported chroma format %d", kvazaar_chroma_format_local);
    }
  } else if (KVZ_BIT_DEPTH == 8) {
    GST_INFO ("This Kvazaar build supports 8-bit depth");
    if (kvazaar_chroma_format_local == 0) {
      set_value (&fmt, 1, "I420");
    } else if (kvazaar_chroma_format_local == KVZ_CSP_420) {
      set_value (&fmt, 1, "I420");
    } else {
      GST_ERROR ("Unsupported chroma format %d", kvazaar_chroma_format_local);
    }
  }

  if (G_VALUE_TYPE (&fmt) != G_TYPE_INVALID)
    gst_structure_take_value (s, "format", &fmt);
}

static GstCaps *
gst_kvazaar_enc_get_supported_input_caps (void)
{
  GstCaps *caps;
  int kvazaar_chroma_format = 0;

  caps = gst_caps_new_simple ("video/x-raw",
      "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, G_MAXINT, 1,
      "width", GST_TYPE_INT_RANGE, 4, G_MAXINT,
      "height", GST_TYPE_INT_RANGE, 4, G_MAXINT, NULL);

  gst_kvazaar_enc_add_kvazaar_chroma_format (gst_caps_get_structure (caps, 0),
      kvazaar_chroma_format);

  GST_DEBUG ("returning %" GST_PTR_FORMAT, caps);
  return caps;
}

static gboolean
gst_kvazaar_enc_sink_query (GstVideoEncoder * enc, GstQuery * query)
{
  gboolean res;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_ACCEPT_CAPS:{
      GstCaps *acceptable, *caps;

      acceptable = gst_kvazaar_enc_get_supported_input_caps ();
      gst_query_parse_accept_caps (query, &caps);

      gst_query_set_accept_caps_result (query,
          gst_caps_is_subset (caps, acceptable));
      gst_caps_unref (acceptable);
      res = TRUE;
    }
      break;
    default:
      res = GST_VIDEO_ENCODER_CLASS (parent_class)->sink_query (enc, query);
      break;
  }

  return res;
}

static GstCaps *
gst_kvazaar_enc_sink_getcaps (GstVideoEncoder * enc, GstCaps * filter)
{
  GstCaps *supported_incaps;
  GstCaps *ret;

  supported_incaps = gst_kvazaar_enc_get_supported_input_caps ();

  ret = gst_video_encoder_proxy_getcaps (enc, supported_incaps, filter);
  if (supported_incaps)
    gst_caps_unref (supported_incaps);
  return ret;
}

static void
gst_kvazaar_enc_class_init (GstKvazaarEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;
  GstVideoEncoderClass *gstencoder_class;

  gobject_class = G_OBJECT_CLASS (klass);
  element_class = GST_ELEMENT_CLASS (klass);
  gstencoder_class = GST_VIDEO_ENCODER_CLASS (klass);

  gobject_class->set_property = gst_kvazaar_enc_set_property;
  gobject_class->get_property = gst_kvazaar_enc_get_property;
  gobject_class->finalize = gst_kvazaar_enc_finalize;

  gstencoder_class->set_format = GST_DEBUG_FUNCPTR (gst_kvazaar_enc_set_format);
  gstencoder_class->handle_frame =
      GST_DEBUG_FUNCPTR (gst_kvazaar_enc_handle_frame);
  gstencoder_class->start = GST_DEBUG_FUNCPTR (gst_kvazaar_enc_start);
  gstencoder_class->stop = GST_DEBUG_FUNCPTR (gst_kvazaar_enc_stop);
  gstencoder_class->flush = GST_DEBUG_FUNCPTR (gst_kvazaar_enc_flush);
  gstencoder_class->finish = GST_DEBUG_FUNCPTR (gst_kvazaar_enc_finish);
  gstencoder_class->getcaps = GST_DEBUG_FUNCPTR (gst_kvazaar_enc_sink_getcaps);
  gstencoder_class->sink_query = GST_DEBUG_FUNCPTR (gst_kvazaar_enc_sink_query);
  gstencoder_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_kvazaar_enc_propose_allocation);

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_uint ("bitrate", "Bitrate", "Bitrate in kbit/sec", 0,
          G_MAXINT, PROP_BITRATE_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
          GST_PARAM_MUTABLE_PLAYING));

  g_object_class_install_property (gobject_class, PROP_QP,
      g_param_spec_int ("qp", "Quantization parameter",
          "QP for P slices in (implied) CQP mode (-1 = disabled)", -1,
          51, PROP_QP_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_INTRA_PERIOD,
      g_param_spec_int ("intra-period", "Intra period",
          "Period of intra pictures (0 = only first picture is intra;"
          " 1 = every picture is intra; 2-N = every Nth picture is intra)", 0,
          64, PROP_INTRA_PERIOD_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VPS_PERIOD,
      g_param_spec_int ("vps-period", "VPS period",
          "Specify how often the video parameter set is re-sent. "
          "(0 = only first picture is intra; N = send VPS with every Nth intra frame", 0,
          64, PROP_VPS_PERIOD_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PRESET,
      g_param_spec_enum ("preset", "Speed preset",
          "Preset name for speed/quality tradeoff options",
          GST_KVAZAAR_ENC_PRESET_TYPE, PROP_PRESET_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

#ifdef HAS_CRYPTO
  g_object_class_install_property (gobject_class, PROP_CRYPTO,
      g_param_spec_flags ("crypto", "Crypto mode",
          "Preset name for enabling selective crypto options",
          GST_KVAZAAR_ENC_CRYPTO_TYPE, KVZ_CRYPTO_OFF,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_KEY,
      g_param_spec_string ("key", "Optional key",
          "String representing the key as an array of 16 uint8 values",
          PROP_KEY_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
#endif

  g_object_class_install_property (gobject_class, PROP_NO_PSNR,
      g_param_spec_boolean ("no-psnr", "No PSNR",
        "Don't calculate PSNR for frames", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_NO_INFO,
      g_param_spec_boolean ("no-info", "No info", "Don't add encoder info SEI",
          FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SOURCE_SCAN_TYPE,
      g_param_spec_enum ("source-scan-type", "Source scan type",
          "Set source scan type",
          GST_KVAZAAR_ENC_SOURCE_SCAN_TYPE_TYPE, GST_KVAZAAR_PROGRESSIVE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUD,
      g_param_spec_boolean ("aud", "Access Unit Delimiters",
        "Use access unit delimiters", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_REF_FRAMES,
      g_param_spec_int ("ref-frames", "Reference frames",
          "Number of reference frames to use "
          "(0 = use Kvazaar default (1) or preset)", 0, 15,
          PROP_REF_FRAMES_DEFAULT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PU_DEPTH_INTRA,
      g_param_spec_string ("pu-depth-intra", "PU depth intra",
          "Range for sizes for intra predictions: <int>-<int> "
          "(0, 1, 2, 3, 4: from 64x64 to 4x4)",
          PROP_PU_DEPTH_INTRA_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PU_DEPTH_INTER,
      g_param_spec_string ("pu-depth-inter", "PU depth inter",
          "Range for sizes for inter predictions: <int>-<int> "
          "(0, 1, 2, 3: from 64x64 to 8x8)",
          PROP_PU_DEPTH_INTER_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RDO,
      g_param_spec_enum ("rdo", "Rate Distorsion calculation",
          "Intra mode search complexity",
          GST_KVAZAAR_ENC_RDO_TYPE, PROP_RDO_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ME,
      g_param_spec_enum ("me", "Motion Estimation",
          "Integer motion estimation",
          GST_KVAZAAR_ENC_ME_TYPE, PROP_ME_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_DEBLOCK,
      g_param_spec_string ("deblock", "Deblocking filter",
          "Set deblocking filter <beta:tc> (beta = -6...6; tc = -6...6). "
          "Can also be yes, true, 1, no, false or 0 to enable or disable deblocking filter.",
          PROP_DEBLOCK_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SIGNHIDE,
      g_param_spec_boolean ("signhide", "Sign Hide",
        "Enable sign hiding.", TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SUBME,
      g_param_spec_enum ("subme", "Sub Motion Estimation",
          "Fractional pixel motion estimation level",
          GST_KVAZAAR_ENC_SUBME_TYPE, GST_KVAZAAER_ENC_SUBME_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SAO,
      g_param_spec_enum ("sao", "Sample adaptive offset",
          "Sample adaptive offset filter",
          GST_KVAZAAR_ENC_SAO_TYPE, GST_KVAZAAER_ENC_SAO_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_RDOQ,
      g_param_spec_boolean ("rdoq", "Rate-Distortion Optimized Quantization",
        "Enable Rate-Distortion Optimized Quantization", TRUE,
        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_RDOQ_SKIP,
      g_param_spec_boolean ("rdoq-skip",
        "Rate-Distortion Optimized Quantization skip",
        "Skips RDOQ for 4x4 blocks", TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_TRSKIP,
      g_param_spec_boolean ("transform-skip", "Transform skip",
        "Enable transform skip (for 4x4 blocks)", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_FULL_INTRA_SEARCH,
      g_param_spec_boolean ("full-intra-search", "Full intra search",
        "Try all intra modes during rough search", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_MV_RDO,
      g_param_spec_boolean ("mv-rdo", "MV RDO",
        "Rate-Distortion Optimized motion vector costs", FALSE,
        G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_SMP,
      g_param_spec_boolean ("smp", "SMP",
        "Symmetric Motion Partition", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_AMP,
      g_param_spec_boolean ("amp", "AMP",
        "Asymmetric Motion Partition", FALSE, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_CU_SPLIT_TERM,
      g_param_spec_enum ("cu-split-termination", "CU split termination",
          "CU split search termination condition",
          GST_KVAZAAR_CU_SPLIT_TERM_TYPE, PROP_CU_SPLIT_TERM_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ME_EARLY_TERM,
      g_param_spec_enum ("me-early-termination", "ME early termination",
          "ME early termination condition",
          GST_KVAZAAR_ME_EARLY_TERM_TYPE, PROP_ME_EARLY_TERM_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_GOP,
      g_param_spec_string ("gop", "Group Of Pictures",
          "Definition of GOP structure "
          "(0 = disabled | "
          "8 = B-frame pyramid of length 8 | "
          "lp-<string>: lp-gop definition (e.g. lp-g8d4t2, see README) )",
          PROP_GOP_DEFAULT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ROI,
      g_param_spec_string ("roi", "Region of Interest",
          "Delta QP map for region of interest, "
          "see Kvazaar manual.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_KVZ_OPTS,
      g_param_spec_string ("option-string", "Option string",
          "String of Kvazaar options, "
          "in the format \"key1=value1,key2=value2\". "
          "Overrides element properties.",
          NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_add_static_pad_template (element_class, &sink_factory);
  gst_element_class_add_static_pad_template (element_class, &src_factory);

  gst_element_class_set_static_metadata (element_class,
      "Kvazaar HEVC/H.265 video encoder", "Codec/Encoder/Video", "HEVC/H.265 encoder",
      "Alexandre Esse <alexandre.esse.dev@gmail.com>");
}

/*
 * Initialization function.
 * Get Kvazaar API and init plugin parameters.
 */
static void
gst_kvazaar_enc_init (GstKvazaarEnc * encoder)
{

  /* Collect functions of the Kvazaar api */
  encoder->api = kvz_api_get (KVZ_BIT_DEPTH);
  if (!encoder->api)
      encoder->api = kvz_api_get (0);

  /* Initialize a config structure with default value*/
  encoder->kvazaarconfig = encoder->api->config_alloc ();
  if (!encoder->api->config_init (encoder->kvazaarconfig))
    GST_ERROR_OBJECT (encoder, "Failed to init config structure");

  encoder->push_header = TRUE;
  encoder->bitrate = PROP_BITRATE_DEFAULT;
  encoder->qp = PROP_QP_DEFAULT;
  encoder->intra_period = PROP_INTRA_PERIOD_DEFAULT;
  encoder->vps_period = PROP_VPS_PERIOD_DEFAULT;
  encoder->no_psnr = FALSE;
  encoder->no_info = FALSE;
  encoder->preset = PROP_PRESET_DEFAULT;
#ifdef HAS_CRYPTO
  encoder->crypto = KVZ_CRYPTO_OFF;
  encoder->key = g_string_new (PROP_KEY_DEFAULT);
#endif
  encoder->source_scan_type = GST_KVAZAAR_PROGRESSIVE;
  encoder->aud_enable = FALSE;
  encoder->ref_frames = PROP_REF_FRAMES_DEFAULT;
  encoder->pu_depth_inter = g_string_new (PROP_PU_DEPTH_INTER_DEFAULT);
  encoder->pu_depth_intra = g_string_new (PROP_PU_DEPTH_INTRA_DEFAULT);
  encoder->rdo = PROP_RDO_DEFAULT;
  encoder->me = PROP_ME_DEFAULT;
  encoder->deblock = g_string_new (PROP_DEBLOCK_DEFAULT);
  encoder->deblock_set = FALSE;
  encoder->signhide = TRUE;
  encoder->signhide_set = FALSE;
  encoder->subme = GST_KVAZAAER_ENC_SUBME_DEFAULT;
  encoder->sao = GST_KVAZAAER_ENC_SAO_DEFAULT;
  encoder->rdoq = TRUE;
  encoder->rdoq_set = FALSE;
  encoder->rdoq_skip = TRUE;
  encoder->rdoq_skip_set = FALSE;
  encoder->trskip = FALSE;
  encoder->trskip_set = FALSE;
  encoder->full_intra_search = FALSE;
  encoder->full_intra_search_set = FALSE;
  encoder->mv_rdo = FALSE;
  encoder->mv_rdo_set = FALSE;
  encoder->smp = FALSE;
  encoder->smp_set = FALSE;
  encoder->amp = FALSE;
  encoder->amp_set = FALSE;
  encoder->cu_split_termination = PROP_CU_SPLIT_TERM_DEFAULT;
  encoder->me_early_termination = PROP_ME_EARLY_TERM_DEFAULT;
  encoder->gop = g_string_new (PROP_GOP_DEFAULT);
  encoder->gop_set = FALSE;
  encoder->roi = g_string_new (NULL);
  encoder->roi_set = FALSE;
  encoder->kvz_opts = g_string_new (NULL);

  encoder->systeme_frame_number_offset = 0;
  GST_DEBUG ("source scan type %u", encoder->kvazaarconfig->source_scan_type);
}

typedef struct
{
  GstVideoCodecFrame *frame;
  GstVideoFrame vframe;
} FrameData;

static FrameData *
gst_kvazaar_enc_queue_frame (GstKvazaarEnc * enc, GstVideoCodecFrame * frame,
    GstVideoInfo * info)
{
  GstVideoFrame vframe;
  FrameData *fdata;

  if (!gst_video_frame_map (&vframe, info, frame->input_buffer, GST_MAP_READ))
    return NULL;

  fdata = g_slice_new (FrameData);
  fdata->frame = gst_video_codec_frame_ref (frame);
  fdata->vframe = vframe;

  enc->pending_frames = g_list_prepend (enc->pending_frames, fdata);

  return fdata;
}

static void
gst_kvazaar_enc_dequeue_frame (GstKvazaarEnc * enc, GstVideoCodecFrame * frame)
{
  GList *l;

  for (l = enc->pending_frames; l; l = l->next) {
    FrameData *fdata = l->data;

    if (fdata->frame != frame)
      continue;

    gst_video_frame_unmap (&fdata->vframe);
    gst_video_codec_frame_unref (fdata->frame);
    g_slice_free (FrameData, fdata);

    enc->pending_frames = g_list_delete_link (enc->pending_frames, l);
    return;
  }
}

static void
gst_kvazaar_enc_dequeue_all_frames (GstKvazaarEnc * enc)
{
  GList *l;

  for (l = enc->pending_frames; l; l = l->next) {
    FrameData *fdata = l->data;

    gst_video_frame_unmap (&fdata->vframe);
    gst_video_codec_frame_unref (fdata->frame);
    g_slice_free (FrameData, fdata);
  }
  g_list_free (enc->pending_frames);
  enc->pending_frames = NULL;
}

static gboolean
gst_kvazaar_enc_start (GstVideoEncoder * encoder)
{

  return TRUE;
}

/*
 * Flush encoder frames and close encoder.
 */
static gboolean
gst_kvazaar_enc_stop (GstVideoEncoder * encoder)
{
  GstKvazaarEnc *kvazaarenc = GST_KVAZAAR_ENC (encoder);

  GST_DEBUG_OBJECT (encoder, "stop encoder");

  gst_kvazaar_enc_flush_frames (kvazaarenc, FALSE);
  gst_kvazaar_enc_close_encoder (kvazaarenc);
  gst_kvazaar_enc_dequeue_all_frames (kvazaarenc);

  if (kvazaarenc->input_state)
    gst_video_codec_state_unref (kvazaarenc->input_state);
  kvazaarenc->input_state = NULL;

  return TRUE;
}

/*
 * Flush encoder frames and restart encoder.
 */
static gboolean
gst_kvazaar_enc_flush (GstVideoEncoder * encoder)
{
  GstKvazaarEnc *kvazaarenc = GST_KVAZAAR_ENC (encoder);

  GST_DEBUG_OBJECT (encoder, "flushing encoder");

  gst_kvazaar_enc_flush_frames (kvazaarenc, FALSE);
  gst_kvazaar_enc_close_encoder (kvazaarenc);
  gst_kvazaar_enc_dequeue_all_frames (kvazaarenc);

  gst_kvazaar_enc_init_encoder (kvazaarenc);

  return TRUE;
}

static void
gst_kvazaar_enc_finalize (GObject * object)
{
  GstKvazaarEnc *encoder = GST_KVAZAAR_ENC (object);

  if (encoder->input_state)
    gst_video_codec_state_unref (encoder->input_state);
  encoder->input_state = NULL;

  gst_kvazaar_enc_close_encoder (encoder);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*
 * GstVideoFormat to Kvazaar format. Set data to the corresponding number of
 * planes.
 */
static gint
gst_kvazaar_enc_gst_to_kvazaar_video_format (GstVideoFormat format, gint * data)
{
  switch (format) {
    case GST_VIDEO_FORMAT_I420:
    case GST_VIDEO_FORMAT_YV12:
    case GST_VIDEO_FORMAT_I420_10LE:
    case GST_VIDEO_FORMAT_I420_10BE:
      if (data)
        *data = 3;
      return KVZ_FORMAT_P420;
    default:
      g_return_val_if_reached (GST_VIDEO_FORMAT_UNKNOWN);
  }
}

/*
 * Initialize Kvazaar encoder.
 * The encoder is created based on a kvz_config struct.
 */
static gboolean
gst_kvazaar_enc_init_encoder (GstKvazaarEnc * encoder)
{
  GstVideoInfo *info;

  if (!encoder->input_state) {
    GST_DEBUG_OBJECT (encoder, "Have no input state yet");
    return FALSE;
  }

  info = &encoder->input_state->info;

  /* Make sure that the encoder is closed */
  gst_kvazaar_enc_close_encoder (encoder);

  GST_OBJECT_LOCK (encoder);

  /* Set up encoder parameters */

  /* First, set up parameters that would not be overwritten by preset */

  encoder->kvazaarconfig->input_format =
    gst_kvazaar_enc_gst_to_kvazaar_video_format (info->finfo->format, NULL);
  encoder->kvazaarconfig->framerate_num = info->fps_n;
  encoder->kvazaarconfig->framerate_denom = info->fps_d;
  encoder->kvazaarconfig->width = info->width;
  encoder->kvazaarconfig->height = info->height;
  encoder->kvazaarconfig->qp = encoder->qp;
  encoder->kvazaarconfig->target_bitrate = encoder->bitrate;
  //* TEST
  encoder->kvazaarconfig->intra_period = encoder->intra_period;
  encoder->kvazaarconfig->vps_period = encoder->vps_period;
  encoder->kvazaarconfig->calc_psnr = encoder->no_psnr ? 0 : 1 ;
  encoder->kvazaarconfig->add_encoder_info = encoder->no_info ? 0 : 1 ;
#ifdef HAS_CRYPTO
  encoder->kvazaarconfig->crypto_features = encoder->crypto;
  encoder->api->config_parse (encoder->kvazaarconfig, "key", encoder->key->str);
  // END TEST */
  /* Disable wavefront parallel processing, currently not supported when
   * selective encryption is on */
  //* TEST
  if (encoder->crypto != KVZ_CRYPTO_OFF)
    encoder->kvazaarconfig->wpp = 0;
#endif
  encoder->kvazaarconfig->aud_enable = encoder->aud_enable ? 1 : 0 ;
  // END TEST */

  /* Then, parse preset configuration only if specified. We don't want to
   * overwrite the previous parameters if no preset is set. */

  //* TEST
  if (encoder->preset != GST_KVAZAAR_ENC_NO_PRESET)
    encoder->api->config_parse (encoder->kvazaarconfig, "preset",
        preset_types[encoder->preset].value_nick);
  // END TEST */

  /* Finally, set parameters that must be overwrite preset if specified */

  //* TEST
  if (encoder->ref_frames != 0)
    encoder->kvazaarconfig->ref_frames = encoder->ref_frames;
  if (g_strcmp0 (encoder->pu_depth_inter->str, ""))
    encoder->api->config_parse (encoder->kvazaarconfig, "pu-depth-inter",
        encoder->pu_depth_inter->str);
  if (g_strcmp0 (encoder->pu_depth_intra->str, ""))
    encoder->api->config_parse (encoder->kvazaarconfig, "pu-depth-intra",
        encoder->pu_depth_intra->str);
  if (encoder->rdo != GST_KVAZAAR_ENC_RDO_DEFAULT)
    encoder->kvazaarconfig->rdo = encoder->rdo;
  if (encoder->me != PROP_ME_DEFAULT)
    encoder->kvazaarconfig->ime_algorithm = encoder->me;
  if (encoder->deblock_set)
    encoder->api->config_parse (encoder->kvazaarconfig, "deblock",
        encoder->deblock->str);
  if (encoder->signhide_set)
    encoder->kvazaarconfig->signhide_enable = encoder->signhide;
  if (encoder->subme != GST_KVAZAAER_ENC_SUBME_DEFAULT)
  encoder->api->config_parse (encoder->kvazaarconfig, "subme",
      subme_types[encoder->subme].value_nick);
  if (encoder->sao != GST_KVAZAAER_ENC_SAO_DEFAULT)
  encoder->api->config_parse (encoder->kvazaarconfig, "sao",
      sao_types[encoder->sao].value_nick);
  if (encoder->rdoq_set)
    encoder->kvazaarconfig->rdoq_enable = encoder->rdoq;
  if (encoder->rdoq_skip_set)
    encoder->kvazaarconfig->rdoq_skip = encoder->rdoq_skip;
  if (encoder->trskip_set)
    encoder->kvazaarconfig->trskip_enable = encoder->trskip;
  if (encoder->full_intra_search_set)
    encoder->kvazaarconfig->full_intra_search = encoder->full_intra_search;
  if (encoder->mv_rdo_set)
    encoder->kvazaarconfig->mv_rdo = encoder->mv_rdo;
  if (encoder->smp_set)
    encoder->kvazaarconfig->smp_enable = encoder->smp;
  if (encoder->amp_set)
    encoder->kvazaarconfig->amp_enable = encoder->amp;
  if (encoder->cu_split_termination != PROP_CU_SPLIT_TERM_DEFAULT)
    encoder->kvazaarconfig->cu_split_termination = encoder->cu_split_termination;
  if (encoder->me_early_termination != PROP_ME_EARLY_TERM_DEFAULT)
    encoder->kvazaarconfig->me_early_termination = encoder->me_early_termination;
  if (encoder->gop_set)
    encoder->api->config_parse (encoder->kvazaarconfig, "gop",
        encoder->gop->str);

  if (encoder->roi_set)
  {
    //encoder->dqps = (int8_t *) g_malloc (sizeof (int8_t *) * 9);
    GST_DEBUG ("Got ROI string: %s", encoder->roi->str);

    parse_roi_array (encoder->roi->str, &encoder->roi_width, &encoder->roi_height,
        &encoder->dqps, -51, 51);
    GST_DEBUG ("%d %d %d", encoder->roi_width, encoder->roi_height, encoder->dqps[0]);
    encoder->kvazaarconfig->roi.width = encoder->roi_width;
    encoder->kvazaarconfig->roi.height = encoder->roi_height;
    encoder->kvazaarconfig->roi.dqps = encoder->dqps;
  }
  // END TEST */

  /* Parse Kvazaar option string property */
  if (encoder->kvz_opts->str != NULL &&
      parse_kvazaar_options (encoder, encoder->kvz_opts->str))
  {
    GST_ERROR ("Error parsing option string");
  }

  encoder->reconfig = FALSE;

  /* good start, will be corrected if needed */
  encoder->dts_offset = 0;

  GST_OBJECT_UNLOCK (encoder);

  /*		Print all parameters values		*/
  GST_DEBUG ("intra period %d", encoder->kvazaarconfig->intra_period);
  GST_DEBUG ("qp %d", encoder->kvazaarconfig->qp);
  GST_DEBUG ("vps_period %d", encoder->kvazaarconfig->vps_period);
  GST_DEBUG ("width %d", encoder->kvazaarconfig->width);
  GST_DEBUG ("height %d", encoder->kvazaarconfig->height);
  GST_DEBUG ("framerate num %d", encoder->kvazaarconfig->framerate_num);
  GST_DEBUG ("framerate denom %d", encoder->kvazaarconfig->framerate_denom);
  GST_DEBUG ("aud_enable %d", encoder->kvazaarconfig->aud_enable);
  GST_DEBUG ("source_scan_type %d", encoder->kvazaarconfig->source_scan_type);
  GST_DEBUG ("ref_frames %d", encoder->kvazaarconfig->ref_frames);
  GST_DEBUG ("rdo %d", encoder->kvazaarconfig->rdo);
  GST_DEBUG ("ime_algorithm %d", encoder->kvazaarconfig->ime_algorithm);
  GST_DEBUG ("deblock enable %d", encoder->kvazaarconfig->deblock_enable);
  GST_DEBUG ("deblock_beta %d", encoder->kvazaarconfig->deblock_beta);
  GST_DEBUG ("deblock_tc %d", encoder->kvazaarconfig->deblock_tc);
  GST_DEBUG ("signhide_enable %d", encoder->kvazaarconfig->signhide_enable);
  GST_DEBUG ("fme_level %d", encoder->kvazaarconfig->fme_level);
  GST_DEBUG ("sao type %d", encoder->kvazaarconfig->sao_type);
  GST_DEBUG ("rdoq_enable %d", encoder->kvazaarconfig->rdoq_enable);
  GST_DEBUG ("smp_enable %d", encoder->kvazaarconfig->smp_enable);
  GST_DEBUG ("amp_enable %d", encoder->kvazaarconfig->amp_enable);
  GST_DEBUG ("full_intra_search %d", encoder->kvazaarconfig->full_intra_search);
  GST_DEBUG ("trskip_enable %d", encoder->kvazaarconfig->trskip_enable);
  GST_DEBUG ("bipred %d", encoder->kvazaarconfig->bipred);
  /*
  GST_DEBUG ("tr_depth_intra %d", encoder->kvazaarconfig->tr_depth_intra);
  GST_DEBUG ("sar_width %d", encoder->kvazaarconfig->vui.sar_width);
  GST_DEBUG ("sar_height %d", encoder->kvazaarconfig->vui.sar_height);
  GST_DEBUG ("overscan %d", encoder->kvazaarconfig->vui.overscan);
  GST_DEBUG ("videoformat %d", encoder->kvazaarconfig->vui.videoformat);
  GST_DEBUG ("fullrange %d", encoder->kvazaarconfig->vui.fullrange);
  GST_DEBUG ("colorprim %d", encoder->kvazaarconfig->vui.colorprim);
  GST_DEBUG ("transfer %d", encoder->kvazaarconfig->vui.transfer);
  GST_DEBUG ("colormatrix %d", encoder->kvazaarconfig->vui.colormatrix);
  GST_DEBUG ("chroma_loc %d", encoder->kvazaarconfig->vui.chroma_loc);
  GST_DEBUG ("tiles_width_count %d", encoder->kvazaarconfig->tiles_width_count);
  GST_DEBUG ("tiles_height_count %d", encoder->kvazaarconfig->tiles_height_count);
  GST_DEBUG ("tiles_width_split %d", encoder->kvazaarconfig->tiles_width_split);
  GST_DEBUG ("tiles_height_split %d", encoder->kvazaarconfig->tiles_height_split);
  GST_DEBUG ("wpp %d", encoder->kvazaarconfig->wpp);
  GST_DEBUG ("owf %d", encoder->kvazaarconfig->owf);
  GST_DEBUG ("slice_count %d", encoder->kvazaarconfig->slice_count);
  GST_DEBUG ("slice_addresses_in_ts %d", encoder->kvazaarconfig->slice_addresses_in_ts);
  GST_DEBUG ("threads %d", encoder->kvazaarconfig->threads);
  GST_DEBUG ("cpuid %d", encoder->kvazaarconfig->cpuid);
  // */
  GST_DEBUG ("pu_depth_inter / min  %d", encoder->kvazaarconfig->pu_depth_inter.min);
  GST_DEBUG ("pu_depth_inter / max %d", encoder->kvazaarconfig->pu_depth_inter.max);
  GST_DEBUG ("pu_depth_intra / min %d", encoder->kvazaarconfig->pu_depth_intra.min);
  GST_DEBUG ("pu_depth_intra / max %d", encoder->kvazaarconfig->pu_depth_intra.max);
  GST_DEBUG ("calc_psnr %d", encoder->kvazaarconfig->calc_psnr);
  GST_DEBUG ("add_encoder_info %d", encoder->kvazaarconfig->add_encoder_info);
  GST_DEBUG ("target_bitrate %d", encoder->kvazaarconfig->target_bitrate);
  GST_DEBUG ("mv_rdo %d", encoder->kvazaarconfig->mv_rdo);
  /*
  GST_DEBUG ("mv_constraint %d", encoder->kvazaarconfig->mv_constraint);
  GST_DEBUG ("hash %d", encoder->kvazaarconfig->hash);
  GST_DEBUG ("optional_key %d", encoder->kvazaarconfig->optional_key);
  GST_DEBUG ("lossless %d", encoder->kvazaarconfig->lossless);
  GST_DEBUG ("tmvp_enable %d", encoder->kvazaarconfig->tmvp_enable);
  // */
  GST_DEBUG ("cu_split_termination %d", encoder->kvazaarconfig->cu_split_termination);
  GST_DEBUG ("me_early_termination %d", encoder->kvazaarconfig->me_early_termination);
  GST_DEBUG ("rdoq_skip %d", encoder->kvazaarconfig->rdoq_skip);
  GST_DEBUG ("input_format %d", encoder->kvazaarconfig->input_format);
  GST_DEBUG ("gop_len %d", encoder->kvazaarconfig->gop_len);
  GST_DEBUG ("gop_lowdelay %d", encoder->kvazaarconfig->gop_lowdelay);
  GST_DEBUG ("gop_lp_definition / d %d", encoder->kvazaarconfig->gop_lp_definition.d);
  GST_DEBUG ("gop_lp_definition / t %d", encoder->kvazaarconfig->gop_lp_definition.t);
  /*
  GST_DEBUG ("input_bitdepth %d", encoder->kvazaarconfig->input_bitdepth);
  GST_DEBUG ("implicit_rdpcm %d", encoder->kvazaarconfig->implicit_rdpcm);
  GST_DEBUG ("roi / width %d", encoder->kvazaarconfig->roi.width);
  GST_DEBUG ("roi / height %d", encoder->kvazaarconfig->roi.height);
  GST_DEBUG ("roi / dqps %d", encoder->kvazaarconfig->roi.dqps);
  GST_DEBUG ("slices %d", encoder->kvazaarconfig->slices);
  GST_DEBUG ("erp_aqp %d", encoder->kvazaarconfig->erp_aqp);
  // */

  /* Open Kvazaar encoder */
  encoder->kvazaarenc = encoder->api->encoder_open (encoder->kvazaarconfig);
  if (!encoder->kvazaarenc) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Can not initialize Kvazaar encoder."), (NULL));
    return FALSE;
  }

  encoder->push_header = TRUE;

  return TRUE;
}

/*
 * Close Kvazaar encoder.
 */
static void
gst_kvazaar_enc_close_encoder (GstKvazaarEnc * encoder)
{
  if (encoder->kvazaarenc != NULL)
  {
    encoder->api->encoder_close (encoder->kvazaarenc);
    encoder->kvazaarenc = NULL;
  }
}

/*
 * Parse a byte stream to a nal.
 *
 * This function removes 0x03 from the start code 0x 00 00 03 in the byte
 * stream. It only returns the first nal by detecting the start code
 * 0x 00 00 00 01.
 */
static kvz_data_chunk *
gst_kvazaar_enc_bytestream_to_nal (kvz_data_chunk * input)
{
  kvz_data_chunk *output;
  int i, j, zeros;

  output = g_malloc (sizeof (kvz_data_chunk));
  output->len = input->len - 4;

  zeros = 0;
  for (i = 4, j = 0; i < input->len; (i++, j++)) {
    if (input->data[i] == 0x00) {
      zeros++;
    } else if (input->data[i] == 0x03 && zeros == 2) {
      zeros = 0;
      j--;
      output->len--;
      continue;
    } else if (input->data[i] == 0x01 && zeros == 3) {
      j -= 3;
      output->len = j;
      break;
    } else {
      zeros = 0;
    }
    output->data[j] = input->data[i];
  }

  return output;
}

/*
 * Set output caps level tier and profile.
 */
static gboolean
gst_kvazaar_enc_set_level_tier_and_profile (GstKvazaarEnc * encoder, GstCaps * caps)
{
  int header_return;
  gboolean ret = TRUE;
  kvz_data_chunk *data_k, *vps_data;
  guint32 size_data;

  GST_DEBUG_OBJECT (encoder, "set profile, level and tier");

  header_return = encoder->api->encoder_headers (encoder->kvazaarenc, &data_k, &size_data);
  if (header_return < 0) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode Kvazaar header failed."),
	("kvazaar_encoder_headers return code=%d", header_return));
    return FALSE;
  }

  GST_DEBUG_OBJECT (encoder, "%d lenght of data in header", size_data);

  /* Get the VPS nal from the header */
  vps_data = gst_kvazaar_enc_bytestream_to_nal (data_k);

  GST_MEMDUMP ("VPS", vps_data->data, vps_data->len);

  if (!gst_codec_utils_h265_caps_set_level_tier_and_profile (caps,
        vps_data->data + 6, vps_data->len - 6)) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode Kvazaar failed."),
        ("Failed to find correct level, tier or profile in VPS"));
    ret = FALSE;
  }

  encoder->api->chunk_free (data_k);
  return ret;
}

/*
static GstBuffer *
gst_kvazaar_enc_get_header_buffer (GstKvazaarEnc * encoder)
{
  kvz_data_chunk *chunks_out;
  guint32 len_out;
  int header_return, i_size, offset;
  GstBuffer *buf;

  header_return = encoder->api->encoder_headers (encoder->kvazaarenc,
      &chunks_out, &len_out);
  if (header_return < 0) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE,
        ("Encode Kvazaar header failed."),
	      ("kvazaar_encoder_headers did not return VPS, SPS and PPS"));
    return FALSE;
  }

  i_size = 0;
  offset = 0;

  if (chunks_out != NULL)
  {
    for (kvz_data_chunk *chunk = chunks_out; chunk != NULL; chunk = chunk->next)
    {
      g_assert (i_size + chunk->len <= len_out);
      i_size += chunk->len;
    }
    buf = gst_buffer_new_allocate (NULL, i_size, NULL);
    for (kvz_data_chunk *chunk = chunks_out; chunk != NULL; chunk = chunk->next)
    {
      gst_buffer_fill (buf, offset, chunks_out->data, chunks_out->len);
      offset += chunk->len;
    }
    return buf;
  }

  encoder->api->chunk_free (chunks_out);

  return NULL;
}
// */

static gboolean
gst_kvazaar_enc_set_src_caps (GstKvazaarEnc * encoder, GstCaps * caps)
{
  GstCaps *outcaps;
  GstStructure *structure;
  GstVideoCodecState *state;
  GstTagList *tags;

  outcaps = gst_caps_new_empty_simple ("video/x-h265");
  structure = gst_caps_get_structure (outcaps, 0);

  gst_structure_set (structure, "stream-format", G_TYPE_STRING, "byte-stream",
      NULL);
  gst_structure_set (structure, "alignment", G_TYPE_STRING, "au", NULL);


  if (!gst_kvazaar_enc_set_level_tier_and_profile (encoder, outcaps)) {
    gst_caps_unref (outcaps);
    return FALSE;
  }

  state = gst_video_encoder_set_output_state (GST_VIDEO_ENCODER (encoder),
      outcaps, encoder->input_state);
  GST_DEBUG_OBJECT (encoder, "output caps: %" GST_PTR_FORMAT, state->caps);
  gst_video_codec_state_unref (state);

  tags = gst_tag_list_new_empty ();
  gst_tag_list_add (tags, GST_TAG_MERGE_REPLACE, GST_TAG_ENCODER, "kvazaar",
      GST_TAG_ENCODER_VERSION, "1.2.0", NULL);
  gst_video_encoder_merge_tags (GST_VIDEO_ENCODER (encoder), tags,
      GST_TAG_MERGE_REPLACE);
  gst_tag_list_unref (tags);

  return TRUE;
}



static void
gst_kvazaar_enc_set_latency (GstKvazaarEnc * encoder)
{
  GstVideoInfo *info = &encoder->input_state->info;
  gint max_delayed_frames;
  GstClockTime latency;

  /* FIXME get a real value from the encoder, this is currently not exposed */
    max_delayed_frames = 5;

  if (info->fps_n) {
    latency = gst_util_uint64_scale_ceil (GST_SECOND * info->fps_d,
        max_delayed_frames, info->fps_n);
  } else {
    /* FIXME: Assume 25fps. This is better than reporting no latency at
     * all and then later failing in live pipelines
     */
    latency = gst_util_uint64_scale_ceil (GST_SECOND * 1,
        max_delayed_frames, 25);
  }

  GST_INFO_OBJECT (encoder,
      "Updating latency to %" GST_TIME_FORMAT " (%d frames)",
      GST_TIME_ARGS (latency), max_delayed_frames);

  gst_video_encoder_set_latency (GST_VIDEO_ENCODER (encoder), latency, latency);
}

/*
 * Flush encoder frames by repeatedly calling the encode function until no frame
 * is returned.
 */
static void
gst_kvazaar_enc_flush_frames (GstKvazaarEnc * encoder, gboolean send)
{
  GstFlowReturn flow_ret;
  guint32 len_out;

  /* first send the remaining frames */
  if (encoder->kvazaarenc)
    do {
      flow_ret = gst_kvazaar_enc_encode_frame (encoder, NULL, NULL, &len_out, send);
    } while (flow_ret == GST_FLOW_OK && len_out > 0);
}

/*
 * Set format and src caps.
 */
static gboolean
gst_kvazaar_enc_set_format (GstVideoEncoder * video_enc,
    GstVideoCodecState * state)
{
  GstKvazaarEnc *encoder = GST_KVAZAAR_ENC (video_enc);
  GstVideoInfo *info = &state->info;

  /* If the encoder is initialized, do not reinitialize it again if not
   * necessary */
  if (encoder->kvazaarenc) {
    GstVideoInfo *old = &encoder->input_state->info;

    if (info->finfo->format == old->finfo->format
        && info->width == old->width && info->height == old->height
        && info->fps_n == old->fps_n && info->fps_d == old->fps_d
        && info->par_n == old->par_n && info->par_d == old->par_d) {
      gst_video_codec_state_unref (encoder->input_state);
      encoder->input_state = gst_video_codec_state_ref (state);
      return TRUE;
    }
    /* clear out pending frames */
    gst_kvazaar_enc_flush_frames (encoder, TRUE);
  }

  if (encoder->input_state)
    gst_video_codec_state_unref (encoder->input_state);
  encoder->input_state = gst_video_codec_state_ref (state);

  if (!gst_kvazaar_enc_init_encoder (encoder))
    return FALSE;

  if (!gst_kvazaar_enc_set_src_caps (encoder, state->caps)) {
    gst_kvazaar_enc_close_encoder (encoder);
    return FALSE;
  }

  gst_kvazaar_enc_set_latency (encoder);

  return TRUE;
}

static GstFlowReturn
gst_kvazaar_enc_finish (GstVideoEncoder * encoder)
{
  GST_DEBUG_OBJECT (encoder, "finish encoder");

  gst_kvazaar_enc_flush_frames (GST_KVAZAAR_ENC (encoder), TRUE);
  gst_kvazaar_enc_flush_frames (GST_KVAZAAR_ENC (encoder), TRUE);
  return GST_FLOW_OK;
}

static gboolean
gst_kvazaar_enc_propose_allocation (GstVideoEncoder * encoder, GstQuery * query)
{
  gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);

  return GST_VIDEO_ENCODER_CLASS (parent_class)->propose_allocation (encoder,
      query);
}

static void
gst_kvazaar_enc_reconfig (GstKvazaarEnc * encoder)
{
  encoder->kvazaarconfig->target_bitrate = encoder->bitrate;
  encoder->reconfig = TRUE;
}

/*
 * Give the input frame to the encoder, and send the frame returned by the
 * encoder if any.
 */
static GstFlowReturn
gst_kvazaar_enc_encode_frame (GstKvazaarEnc * encoder, kvz_picture * cur_in_img,
    GstVideoCodecFrame * input_frame, guint32 * len_out, gboolean send)
{
  GstVideoCodecFrame *frame = NULL;
  kvz_picture *img_rec = NULL;
  GstBuffer *out_buf = NULL;
  kvz_frame_info info_out;
  kvz_data_chunk *chunks_out;
  int offset;
  int encoder_return;
  guint32 out_frame_num, intra_period; // Picture order count
  GstFlowReturn ret = GST_FLOW_OK;
  gboolean update_latency = FALSE;

  if (G_UNLIKELY (encoder->kvazaarenc == NULL)) {
    if (input_frame)
      gst_video_codec_frame_unref (input_frame);
    return GST_FLOW_NOT_NEGOTIATED;
  }

  GST_OBJECT_LOCK (encoder);
  if (encoder->reconfig) {
    // kvazaar_encoder_reconfig is not yet implemented thus we shut down and re-create encoder
    gst_kvazaar_enc_init_encoder (encoder);
    update_latency = TRUE;
  }

 /* if (cur_in_img && input_frame) {
    if (GST_VIDEO_CODEC_FRAME_IS_FORCE_KEYFRAME (input_frame)) {
      GST_INFO_OBJECT (encoder, "Forcing key frame");
      info_out.slice_type = KVZ_SLICE_B;
    }
  }*/
  GST_OBJECT_UNLOCK (encoder);

  if (G_UNLIKELY (update_latency))
    gst_kvazaar_enc_set_latency (encoder);

  encoder_return = encoder->api->encoder_encode (encoder->kvazaarenc,
      cur_in_img, &chunks_out, len_out, &img_rec, NULL, &info_out);

  GST_DEBUG_OBJECT (encoder, "encoder result (%d) with lenght data = %u ",
      encoder_return, *len_out);

  if (encoder_return < 0) {
    GST_ELEMENT_ERROR (encoder, STREAM, ENCODE, ("Encode Kvazaar frame failed."),
        ("kvazaar_encoder_encode return code=%d", encoder_return));
    ret = GST_FLOW_ERROR;
    /* Make sure we finish this frame */
    frame = input_frame;
    goto out;
  }

  GST_DEBUG_OBJECT (encoder, "Frame info: QP=%d", info_out.qp);

  /*GST_LOG_OBJECT (encoder,
      "img_rec dts %" G_GINT64_FORMAT " img_rec pts %" G_GINT64_FORMAT,
      (gint64) img_rec.dts, (gint64) img_rec.pts);
  */
  //*
  if (input_frame)
    gst_video_codec_frame_unref (input_frame);
  // */

  if (!*len_out)
  {
    ret = GST_FLOW_OK;
    GST_LOG_OBJECT (encoder, "no output yet");
    goto out;
  }

  /* Determine system frame number based on poc */
  out_frame_num = GPOINTER_TO_INT (info_out.poc);
  intra_period = encoder->kvazaarconfig->intra_period;

  /* If encoder->kvazaarconfig->intra_period is none 0, we need to keep track of
   * the frame number.*/
  if (intra_period > 0)
  {
    if (out_frame_num == 0 && input_frame &&
        GPOINTER_TO_INT (input_frame->system_frame_number) >= intra_period)
      encoder->systeme_frame_number_offset += intra_period;
    out_frame_num += encoder->systeme_frame_number_offset;
  }

  frame = gst_video_encoder_get_frame (GST_VIDEO_ENCODER (encoder), out_frame_num);
  //g_assert (frame || !send);

  GST_DEBUG_OBJECT (encoder,
      "output picture ready POC=%d system=%d frame found %d",
      GPOINTER_TO_INT (info_out.poc),
      out_frame_num, frame != NULL);

  if (!send || !frame) {
    GST_LOG_OBJECT (encoder, "not sending (%d) or frame not found (%d)", send,
        frame != NULL);
    ret = GST_FLOW_OK;
    goto out;
  }

  offset = 0;

  if (chunks_out != NULL)
  {
    kvz_data_chunk *chunk;
    out_buf = gst_buffer_new_allocate (NULL, *len_out, NULL);
    for (chunk = chunks_out; chunk != NULL; chunk = chunk->next)
    {
      g_assert (offset + chunk->len <= *len_out);
      gst_buffer_fill (out_buf, offset, chunk->data, chunk->len);
      offset += chunk->len;
    }
  }

  encoder->api->chunk_free (chunks_out);

  frame->output_buffer = out_buf;

  /* I think that Kvazaar already outputs header on his own, so we might not
   * need to push it manually */
  /*
  if (encoder->push_header) {
    GstBuffer *header;

    header = gst_kvazaar_enc_get_header_buffer (encoder);
    frame->output_buffer = gst_buffer_append (header, frame->output_buffer);
    encoder->push_header = FALSE;
  }
  // */

  //GST_DEBUG (" output pts %" G_GINT64_FORMAT " output dts %" G_GINT64_FORMAT,
      //(gint64) img_rec->pts, (gint64) img_rec->dts);

  frame->dts = img_rec->dts;

  if (cur_in_img)
    encoder->api->picture_free (cur_in_img);
  encoder->api->picture_free (img_rec);

out:
  if (frame) {
    gst_kvazaar_enc_dequeue_frame (encoder, frame);
    ret = gst_video_encoder_finish_frame (GST_VIDEO_ENCODER (encoder), frame);
  }

  return ret;
}

/*
 * Handle input frame.
 */
static GstFlowReturn gst_kvazaar_enc_handle_frame (GstVideoEncoder * video_enc,
 GstVideoCodecFrame * frame)
{
  GstKvazaarEnc *encoder = GST_KVAZAAR_ENC (video_enc);
  GstVideoInfo *info = &encoder->input_state->info;
  GstFlowReturn ret;
  kvz_picture *cur_in_img = NULL;
  FrameData *fdata;
  gint nplanes = 0;
  guint32 len_out;
  gint chroma_format;

  /*Retrieve the chroma format of the source*/
  chroma_format =
    gst_kvazaar_enc_gst_to_kvazaar_video_format (info->finfo->format, &nplanes);

  if (nplanes != 3)
    goto invalid_format;

  /*Allocate a kvz picture for the input image*/
  cur_in_img =
    encoder->api->picture_alloc_csp (chroma_format,info->width, info->height );

  if (G_UNLIKELY (encoder->kvazaarenc == NULL))
    goto not_inited;

  fdata = gst_kvazaar_enc_queue_frame (encoder, frame, info);
  if (!fdata)
    goto invalid_frame;

  //for (int i = 0; i < nplanes; i++) {
    //cur_in_img->data[i] = GST_VIDEO_FRAME_PLANE_DATA (&fdata->vframe, i);
  //}
  cur_in_img->y = GST_VIDEO_FRAME_PLANE_DATA (&fdata->vframe, 0);
  cur_in_img->u = GST_VIDEO_FRAME_PLANE_DATA (&fdata->vframe, 1);
  cur_in_img->v = GST_VIDEO_FRAME_PLANE_DATA (&fdata->vframe, 2);

  cur_in_img->stride = GST_VIDEO_FRAME_COMP_STRIDE (&fdata->vframe, 0);

  cur_in_img->pts = frame->pts;
  cur_in_img->dts = frame->dts;
  cur_in_img->width = info->width;
  cur_in_img->height = info->height;
  cur_in_img->interlacing = info->interlace_mode;

  /* Interlacing / Width,Height / Chroma format */
  /*
  GST_DEBUG ("kvz_interlacing %d", cur_in_img->interlacing);
  GST_DEBUG ("width %d, height %d", cur_in_img->width,cur_in_img->height);
  GST_DEBUG ("chroma format %d", chroma_format);
  // */

  ret = gst_kvazaar_enc_encode_frame (encoder, cur_in_img, frame, &len_out, TRUE);

  return ret;

/* ERRORS */
invalid_format:
  {
    GST_ERROR_OBJECT (encoder, "Invalid format");
    return GST_FLOW_ERROR;
  }
invalid_frame:
  {
    GST_ERROR_OBJECT (encoder, "Failed to map frame");
    return GST_FLOW_ERROR;
  }

not_inited:
  {
    GST_WARNING_OBJECT (encoder, "Got buffer before set_caps was called");
    return GST_FLOW_NOT_NEGOTIATED;
  }
}

static void
gst_kvazaar_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstKvazaarEnc *encoder;
  GstState state;

  encoder = GST_KVAZAAR_ENC (object);

  GST_OBJECT_LOCK (encoder);

  state = GST_STATE (encoder);
  if ((state != GST_STATE_READY && state != GST_STATE_NULL) &&
      !(pspec->flags & GST_PARAM_MUTABLE_PLAYING))
    goto wrong_state;

  switch (prop_id) {
    case PROP_BITRATE:
      encoder->bitrate = g_value_get_uint (value);
      break;
    case PROP_QP:
      encoder->qp = g_value_get_int (value);
      break;
    case PROP_INTRA_PERIOD:
      encoder->intra_period = g_value_get_int (value);
      break;
    case PROP_VPS_PERIOD:
      encoder->vps_period = g_value_get_int (value);
      break;
    case PROP_NO_PSNR:
      encoder->no_psnr = g_value_get_boolean (value);
      break;
    case PROP_NO_INFO:
      encoder->no_info = g_value_get_boolean (value);
      break;
    case PROP_PRESET:
      encoder->preset = g_value_get_enum (value);
      break;
#ifdef HAS_CRYPTO
    case PROP_CRYPTO:
      encoder->crypto = g_value_get_flags (value);
      break;
    case PROP_KEY:
      g_string_assign (encoder->key, g_value_get_string (value));
      break;
#endif
    case PROP_SOURCE_SCAN_TYPE:
      encoder->source_scan_type = g_value_get_enum (value);
      break;
    case PROP_AUD:
      encoder->aud_enable = g_value_get_boolean (value);
      break;
    case PROP_REF_FRAMES:
      encoder->ref_frames = g_value_get_int (value);
      break;
    case PROP_PU_DEPTH_INTER:
      g_string_assign (encoder->pu_depth_inter, g_value_get_string (value));
      break;
    case PROP_PU_DEPTH_INTRA:
      g_string_assign (encoder->pu_depth_intra, g_value_get_string (value));
      break;
    case PROP_RDO:
      encoder->rdo = g_value_get_enum (value);
      break;
    case PROP_ME:
      encoder->me = g_value_get_enum (value);
      break;
    case PROP_DEBLOCK:
      g_string_assign (encoder->deblock, g_value_get_string (value));
      encoder->deblock_set = TRUE;
      break;
    case PROP_SIGNHIDE:
      encoder->signhide = g_value_get_boolean (value);
      encoder->signhide_set = TRUE;
      break;
    case PROP_SUBME:
      encoder->subme = g_value_get_enum (value);
      break;
    case PROP_SAO:
      encoder->sao = g_value_get_enum (value);
      break;
    case PROP_RDOQ:
      encoder->rdoq = g_value_get_boolean (value);
      encoder->rdoq_set = TRUE;
      break;
    case PROP_RDOQ_SKIP:
      encoder->rdoq_skip = g_value_get_boolean (value);
      encoder->rdoq_skip_set = TRUE;
      break;
    case PROP_TRSKIP:
      encoder->trskip = g_value_get_boolean (value);
      encoder->trskip_set = TRUE;
      break;
    case PROP_FULL_INTRA_SEARCH:
      encoder->full_intra_search = g_value_get_boolean (value);
      encoder->full_intra_search_set = TRUE;
      break;
    case PROP_MV_RDO:
      encoder->mv_rdo = g_value_get_boolean (value);
      encoder->mv_rdo_set = TRUE;
      break;
    case PROP_SMP:
      encoder->smp = g_value_get_boolean (value);
      encoder->smp_set = TRUE;
      break;
    case PROP_AMP:
      encoder->amp = g_value_get_boolean (value);
      encoder->amp_set = TRUE;
      break;
    case PROP_CU_SPLIT_TERM:
      encoder->cu_split_termination = g_value_get_enum (value);
      break;
    case PROP_ME_EARLY_TERM:
      encoder->me_early_termination = g_value_get_enum (value);
      break;
    case PROP_GOP:
      g_string_assign (encoder->gop, g_value_get_string (value));
      encoder->gop_set = TRUE;
      break;
    case PROP_ROI:
      g_string_assign (encoder->roi, g_value_get_string (value));
      encoder->roi_set = TRUE;
      break;
    case PROP_KVZ_OPTS:
      g_string_assign (encoder->kvz_opts, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  gst_kvazaar_enc_reconfig (encoder);
  GST_OBJECT_UNLOCK (encoder);
  return;

wrong_state:
  {
    GST_WARNING_OBJECT (encoder, "setting property in wrong state");
    GST_OBJECT_UNLOCK (encoder);
  }
}

static void
gst_kvazaar_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstKvazaarEnc *encoder;

  encoder = GST_KVAZAAR_ENC (object);

  GST_OBJECT_LOCK (encoder);
  switch (prop_id) {
    case PROP_BITRATE:
      g_value_set_uint (value, encoder->bitrate);
      break;
    case PROP_QP:
      g_value_set_int (value, encoder->qp);
      break;
    case PROP_INTRA_PERIOD:
      g_value_set_int (value, encoder->intra_period);
      break;
    case PROP_VPS_PERIOD:
      g_value_set_int (value, encoder->vps_period);
      break;
    case PROP_NO_PSNR:
      g_value_set_boolean (value, encoder->no_psnr);
      break;
    case PROP_NO_INFO:
      g_value_set_boolean (value, encoder->no_info);
      break;
    case PROP_PRESET:
      g_value_set_enum (value, encoder->preset);
      break;
#ifdef HAS_CRYPTO
    case PROP_CRYPTO:
      g_value_set_flags (value, encoder->crypto);
      break;
    case PROP_KEY:
      g_value_set_string (value, encoder->key->str);
      break;
#endif
    case PROP_SOURCE_SCAN_TYPE:
      g_value_set_enum (value, encoder->source_scan_type);
      break;
    case PROP_AUD:
      g_value_set_boolean (value, encoder->aud_enable);
      break;
    case PROP_REF_FRAMES:
      g_value_set_int (value, encoder->ref_frames);
      break;
    case PROP_PU_DEPTH_INTER:
      g_value_set_string (value, encoder->pu_depth_inter->str);
      break;
    case PROP_PU_DEPTH_INTRA:
      g_value_set_string (value, encoder->pu_depth_intra->str);
      break;
    case PROP_RDO:
      g_value_set_enum (value, encoder->rdo);
      break;
    case PROP_ME:
      g_value_set_enum (value, encoder->me);
      break;
    case PROP_DEBLOCK:
      g_value_set_string (value, encoder->deblock->str);
      break;
    case PROP_SIGNHIDE:
      g_value_set_boolean (value, encoder->signhide);
      break;
    case PROP_SUBME:
      g_value_set_enum (value, encoder->subme);
      break;
    case PROP_SAO:
      g_value_set_enum (value, encoder->sao);
      break;
    case PROP_RDOQ:
      g_value_set_boolean (value, encoder->rdoq);
      break;
    case PROP_RDOQ_SKIP:
      g_value_set_boolean (value, encoder->rdoq_skip);
      break;
    case PROP_TRSKIP:
      g_value_set_boolean (value, encoder->trskip);
      break;
    case PROP_FULL_INTRA_SEARCH:
      g_value_set_boolean (value, encoder->full_intra_search);
      break;
    case PROP_MV_RDO:
      g_value_set_boolean (value, encoder->mv_rdo);
      break;
    case PROP_SMP:
      g_value_set_boolean (value, encoder->smp);
      break;
    case PROP_AMP:
      g_value_set_boolean (value, encoder->amp);
      break;
    case PROP_CU_SPLIT_TERM:
      g_value_set_enum (value, encoder->cu_split_termination);
      break;
    case PROP_ME_EARLY_TERM:
      g_value_set_enum (value, encoder->me_early_termination);
      break;
    case PROP_GOP:
      g_value_set_string (value, encoder->gop->str);
      break;
    case PROP_ROI:
      g_value_set_string (value, encoder->roi->str);
      break;
    case PROP_KVZ_OPTS:
      g_value_set_string (value, encoder->kvz_opts->str);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (encoder);
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (kvazaar_enc_debug, "kvazaarenc", 0,
      "HEVC/H.265 encoding element");

  return gst_element_register (plugin, "kvazaarenc",
      GST_RANK_SECONDARY, GST_TYPE_KVAZAAR_ENC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    kvazaarenc,
    "Plugin based on Kvazaar HEVC/H.265 video encoder",
    plugin_init, GST_PLUGIN_VERSION, GST_PLUGIN_LICENSE, PACKAGE, GST_PLUGIN_ORIGIN)
