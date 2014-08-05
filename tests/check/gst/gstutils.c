/* GStreamer
 * Copyright (C) <2005> Thomas Vander Stichele <thomas at apestaart dot org>
 * Copyright (C) <2006> Tim-Philipp Müller <tim centricular net>
 *
 * gstutils.c: Unit test for functions in gstutils
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>

#define SPECIAL_POINTER(x) ((void*)(19283847+(x)))

static int n_data_probes = 0;
static int n_buffer_probes = 0;
static int n_event_probes = 0;

static GstPadProbeReturn
probe_do_nothing (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstMiniObject *obj = GST_PAD_PROBE_INFO_DATA (info);
  GST_DEBUG_OBJECT (pad, "is buffer:%d", GST_IS_BUFFER (obj));
  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
data_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstMiniObject *obj = GST_PAD_PROBE_INFO_DATA (info);
  n_data_probes++;
  GST_DEBUG_OBJECT (pad, "data probe %d", n_data_probes);
  g_assert (GST_IS_BUFFER (obj) || GST_IS_EVENT (obj));
  g_assert (data == SPECIAL_POINTER (0));
  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
buffer_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstBuffer *obj = GST_PAD_PROBE_INFO_BUFFER (info);
  n_buffer_probes++;
  GST_DEBUG_OBJECT (pad, "buffer probe %d", n_buffer_probes);
  g_assert (GST_IS_BUFFER (obj));
  g_assert (data == SPECIAL_POINTER (1));
  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
event_probe (GstPad * pad, GstPadProbeInfo * info, gpointer data)
{
  GstEvent *obj = GST_PAD_PROBE_INFO_EVENT (info);
  n_event_probes++;
  GST_DEBUG_OBJECT (pad, "event probe %d [%s]",
      n_event_probes, GST_EVENT_TYPE_NAME (obj));
  g_assert (GST_IS_EVENT (obj));
  g_assert (data == SPECIAL_POINTER (2));
  return GST_PAD_PROBE_OK;
}

GST_START_TEST (test_buffer_probe_n_times)
{
  GstElement *pipeline, *fakesrc, *fakesink;
  GstBus *bus;
  GstMessage *message;
  GstPad *pad;

  pipeline = gst_element_factory_make ("pipeline", NULL);
  fakesrc = gst_element_factory_make ("fakesrc", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  g_object_set (fakesrc, "num-buffers", (int) 10, NULL);

  gst_bin_add_many (GST_BIN (pipeline), fakesrc, fakesink, NULL);
  gst_element_link (fakesrc, fakesink);

  pad = gst_element_get_static_pad (fakesink, "sink");

  /* add the probes we need for the test */
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_DATA_BOTH, data_probe,
      SPECIAL_POINTER (0), NULL);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, buffer_probe,
      SPECIAL_POINTER (1), NULL);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_BOTH, event_probe,
      SPECIAL_POINTER (2), NULL);

  /* add some string probes just to test that the data is free'd
   * properly as it should be */
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_DATA_BOTH, probe_do_nothing,
      g_strdup ("data probe string"), (GDestroyNotify) g_free);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER, probe_do_nothing,
      g_strdup ("buffer probe string"), (GDestroyNotify) g_free);
  gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_BOTH, probe_do_nothing,
      g_strdup ("event probe string"), (GDestroyNotify) g_free);

  gst_object_unref (pad);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus (pipeline);
  message = gst_bus_poll (bus, GST_MESSAGE_EOS, -1);
  gst_message_unref (message);
  gst_object_unref (bus);

  g_assert (n_buffer_probes == 10);     /* one for every buffer */
  g_assert (n_event_probes == 4);       /* stream-start, new segment, latency and eos */
  g_assert (n_data_probes == 14);       /* duh */

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  /* make sure nothing was sent in addition to the above when shutting down */
  g_assert (n_buffer_probes == 10);     /* one for every buffer */
  g_assert (n_event_probes == 4);       /* stream-start, new segment, latency and eos */
  g_assert (n_data_probes == 14);       /* duh */
} GST_END_TEST;

static int n_data_probes_once = 0;
static int n_buffer_probes_once = 0;
static int n_event_probes_once = 0;

static GstPadProbeReturn
data_probe_once (GstPad * pad, GstPadProbeInfo * info, guint * data)
{
  GstMiniObject *obj = GST_PAD_PROBE_INFO_DATA (info);

  n_data_probes_once++;
  g_assert (GST_IS_BUFFER (obj) || GST_IS_EVENT (obj));

  gst_pad_remove_probe (pad, *data);

  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
buffer_probe_once (GstPad * pad, GstPadProbeInfo * info, guint * data)
{
  GstBuffer *obj = GST_PAD_PROBE_INFO_BUFFER (info);

  n_buffer_probes_once++;
  g_assert (GST_IS_BUFFER (obj));

  gst_pad_remove_probe (pad, *data);

  return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn
event_probe_once (GstPad * pad, GstPadProbeInfo * info, guint * data)
{
  GstEvent *obj = GST_PAD_PROBE_INFO_EVENT (info);

  n_event_probes_once++;
  g_assert (GST_IS_EVENT (obj));

  gst_pad_remove_probe (pad, *data);

  return GST_PAD_PROBE_OK;
}

GST_START_TEST (test_buffer_probe_once)
{
  GstElement *pipeline, *fakesrc, *fakesink;
  GstBus *bus;
  GstMessage *message;
  GstPad *pad;
  guint id1, id2, id3;

  pipeline = gst_element_factory_make ("pipeline", NULL);
  fakesrc = gst_element_factory_make ("fakesrc", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);

  g_object_set (fakesrc, "num-buffers", (int) 10, NULL);

  gst_bin_add_many (GST_BIN (pipeline), fakesrc, fakesink, NULL);
  gst_element_link (fakesrc, fakesink);

  pad = gst_element_get_static_pad (fakesink, "sink");
  id1 =
      gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_DATA_BOTH,
      (GstPadProbeCallback) data_probe_once, &id1, NULL);
  id2 =
      gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_BUFFER,
      (GstPadProbeCallback) buffer_probe_once, &id2, NULL);
  id3 =
      gst_pad_add_probe (pad, GST_PAD_PROBE_TYPE_EVENT_BOTH,
      (GstPadProbeCallback) event_probe_once, &id3, NULL);
  gst_object_unref (pad);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  bus = gst_element_get_bus (pipeline);
  message = gst_bus_poll (bus, GST_MESSAGE_EOS, -1);
  gst_message_unref (message);
  gst_object_unref (bus);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);

  g_assert (n_buffer_probes_once == 1); /* can we hit it and quit? */
  g_assert (n_event_probes_once == 1);  /* i said, can we hit it and quit? */
  g_assert (n_data_probes_once == 1);   /* let's hit it and quit!!! */
} GST_END_TEST;

GST_START_TEST (test_math_scale)
{
  fail_if (gst_util_uint64_scale_int (1, 1, 1) != 1);

  fail_if (gst_util_uint64_scale_int (10, 10, 1) != 100);
  fail_if (gst_util_uint64_scale_int (10, 10, 2) != 50);

  fail_if (gst_util_uint64_scale_int (0, 10, 2) != 0);
  fail_if (gst_util_uint64_scale_int (0, 0, 2) != 0);

  fail_if (gst_util_uint64_scale_int (G_MAXUINT32, 5, 1) != G_MAXUINT32 * 5LL);
  fail_if (gst_util_uint64_scale_int (G_MAXUINT32, 10, 2) != G_MAXUINT32 * 5LL);

  fail_if (gst_util_uint64_scale_int (G_MAXUINT32, 1, 5) != G_MAXUINT32 / 5LL);
  fail_if (gst_util_uint64_scale_int (G_MAXUINT32, 2, 10) != G_MAXUINT32 / 5LL);

  /* not quite overflow */
  fail_if (gst_util_uint64_scale_int (G_MAXUINT64 - 1, 10,
          10) != G_MAXUINT64 - 1);
  fail_if (gst_util_uint64_scale_int (G_MAXUINT64 - 1, G_MAXINT32,
          G_MAXINT32) != G_MAXUINT64 - 1);
  fail_if (gst_util_uint64_scale_int (G_MAXUINT64 - 100, G_MAXINT32,
          G_MAXINT32) != G_MAXUINT64 - 100);

  /* overflow */
  fail_if (gst_util_uint64_scale_int (G_MAXUINT64 - 1, 10, 1) != G_MAXUINT64);
  fail_if (gst_util_uint64_scale_int (G_MAXUINT64 - 1, G_MAXINT32,
          1) != G_MAXUINT64);

} GST_END_TEST;

GST_START_TEST (test_math_scale_round)
{
  fail_if (gst_util_uint64_scale_int_round (2, 1, 2) != 1);
  fail_if (gst_util_uint64_scale_int_round (3, 1, 2) != 2);
  fail_if (gst_util_uint64_scale_int_round (4, 1, 2) != 2);

  fail_if (gst_util_uint64_scale_int_round (200, 100, 20000) != 1);
  fail_if (gst_util_uint64_scale_int_round (299, 100, 20000) != 1);
  fail_if (gst_util_uint64_scale_int_round (300, 100, 20000) != 2);
  fail_if (gst_util_uint64_scale_int_round (301, 100, 20000) != 2);
  fail_if (gst_util_uint64_scale_int_round (400, 100, 20000) != 2);
} GST_END_TEST;

GST_START_TEST (test_math_scale_ceil)
{
  fail_if (gst_util_uint64_scale_int_ceil (2, 1, 2) != 1);
  fail_if (gst_util_uint64_scale_int_ceil (3, 1, 2) != 2);
  fail_if (gst_util_uint64_scale_int_ceil (4, 1, 2) != 2);

  fail_if (gst_util_uint64_scale_int_ceil (200, 100, 20000) != 1);
  fail_if (gst_util_uint64_scale_int_ceil (299, 100, 20000) != 2);
  fail_if (gst_util_uint64_scale_int_ceil (300, 100, 20000) != 2);
  fail_if (gst_util_uint64_scale_int_ceil (301, 100, 20000) != 2);
  fail_if (gst_util_uint64_scale_int_ceil (400, 100, 20000) != 2);
} GST_END_TEST;

GST_START_TEST (test_math_scale_uint64)
{
  fail_if (gst_util_uint64_scale (1, 1, 1) != 1);

  fail_if (gst_util_uint64_scale (10, 10, 1) != 100);
  fail_if (gst_util_uint64_scale (10, 10, 2) != 50);

  fail_if (gst_util_uint64_scale (0, 10, 2) != 0);
  fail_if (gst_util_uint64_scale (0, 0, 2) != 0);

  fail_if (gst_util_uint64_scale (G_MAXUINT32, 5, 1) != G_MAXUINT32 * 5LL);
  fail_if (gst_util_uint64_scale (G_MAXUINT32, 10, 2) != G_MAXUINT32 * 5LL);

  fail_if (gst_util_uint64_scale (G_MAXUINT32, 1, 5) != G_MAXUINT32 / 5LL);
  fail_if (gst_util_uint64_scale (G_MAXUINT32, 2, 10) != G_MAXUINT32 / 5LL);

  /* not quite overflow */
  fail_if (gst_util_uint64_scale (G_MAXUINT64 - 1, 10, 10) != G_MAXUINT64 - 1);
  fail_if (gst_util_uint64_scale (G_MAXUINT64 - 1, G_MAXUINT32,
          G_MAXUINT32) != G_MAXUINT64 - 1);
  fail_if (gst_util_uint64_scale (G_MAXUINT64 - 100, G_MAXUINT32,
          G_MAXUINT32) != G_MAXUINT64 - 100);

  fail_if (gst_util_uint64_scale (G_MAXUINT64 - 1, 10, 10) != G_MAXUINT64 - 1);
  fail_if (gst_util_uint64_scale (G_MAXUINT64 - 1, G_MAXUINT64,
          G_MAXUINT64) != G_MAXUINT64 - 1);
  fail_if (gst_util_uint64_scale (G_MAXUINT64 - 100, G_MAXUINT64,
          G_MAXUINT64) != G_MAXUINT64 - 100);

  /* overflow */
  fail_if (gst_util_uint64_scale (G_MAXUINT64 - 1, 10, 1) != G_MAXUINT64);
  fail_if (gst_util_uint64_scale (G_MAXUINT64 - 1, G_MAXUINT64,
          1) != G_MAXUINT64);

} GST_END_TEST;

GST_START_TEST (test_math_scale_random)
{
  guint64 val, num, denom, res;
  GRand *rand;
  gint i;

  rand = g_rand_new ();

  i = 100000;
  while (i--) {
    guint64 check, diff;

    val = ((guint64) g_rand_int (rand)) << 32 | g_rand_int (rand);
    num = ((guint64) g_rand_int (rand)) << 32 | g_rand_int (rand);
    denom = ((guint64) g_rand_int (rand)) << 32 | g_rand_int (rand);

    res = gst_util_uint64_scale (val, num, denom);
    check = gst_gdouble_to_guint64 (gst_guint64_to_gdouble (val) *
        gst_guint64_to_gdouble (num) / gst_guint64_to_gdouble (denom));

    if (res < G_MAXUINT64 && check < G_MAXUINT64) {
      if (res > check)
        diff = res - check;
      else
        diff = check - res;

      /* some arbitrary value, really.. someone do the proper math to get
       * the upper bound */
      if (diff > 20000)
        fail_if (diff > 20000);
    }
  }
  g_rand_free (rand);

}

GST_END_TEST;

GST_START_TEST (test_guint64_to_gdouble)
{
  guint64 from[] = { 0, 1, 100, 10000, (guint64) (1) << 63,
    ((guint64) (1) << 63) + 1,
    ((guint64) (1) << 63) + (G_GINT64_CONSTANT (1) << 62)
  };
  gdouble to[] = { 0., 1., 100., 10000., 9223372036854775808.,
    9223372036854775809., 13835058055282163712.
  };
  gdouble tolerance[] = { 0., 0., 0., 0., 0., 1., 1. };
  gint i;
  gdouble result;
  gdouble delta;

  for (i = 0; i < G_N_ELEMENTS (from); ++i) {
    result = gst_util_guint64_to_gdouble (from[i]);
    delta = ABS (to[i] - result);
    fail_unless (delta <= tolerance[i],
        "Could not convert %d: %" G_GUINT64_FORMAT
        " -> %f, got %f instead, delta of %e with tolerance of %e",
        i, from[i], to[i], result, delta, tolerance[i]);
  }
}

GST_END_TEST;

GST_START_TEST (test_gdouble_to_guint64)
{
  gdouble from[] = { 0., 1., 100., 10000., 9223372036854775808.,
    9223372036854775809., 13835058055282163712.
  };
  guint64 to[] = { 0, 1, 100, 10000, (guint64) (1) << 63,
    ((guint64) (1) << 63) + 1,
    ((guint64) (1) << 63) + (G_GINT64_CONSTANT (1) << 62)
  };
  guint64 tolerance[] = { 0, 0, 0, 0, 0, 1, 1 };
  gint i;
  gdouble result;
  guint64 delta;

  for (i = 0; i < G_N_ELEMENTS (from); ++i) {
    result = gst_util_gdouble_to_guint64 (from[i]);
    delta = ABS (to[i] - result);
    fail_unless (delta <= tolerance[i],
        "Could not convert %f: %" G_GUINT64_FORMAT
        " -> %d, got %d instead, delta of %e with tolerance of %e",
        i, from[i], to[i], result, delta, tolerance[i]);
  }
}

GST_END_TEST;

#ifndef GST_DISABLE_PARSE
GST_START_TEST (test_parse_bin_from_description)
{
  struct
  {
    const gchar *bin_desc;
    const gchar *pad_names;
  } bin_tests[] = {
    {
    "identity", "identity0/sink,identity0/src"}, {
    "identity ! identity ! identity", "identity1/sink,identity3/src"}, {
    "identity ! fakesink", "identity4/sink"}, {
    "fakesrc ! identity", "identity5/src"}, {
    "fakesrc ! fakesink", ""}
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (bin_tests); ++i) {
    GstElement *bin, *parent;
    GString *s;
    GstPad *ghost_pad, *target_pad;
    GError *err = NULL;

    bin = gst_parse_bin_from_description (bin_tests[i].bin_desc, TRUE, &err);
    if (err) {
      g_error ("ERROR in gst_parse_bin_from_description (%s): %s",
          bin_tests[i].bin_desc, err->message);
    }
    g_assert (bin != NULL);

    s = g_string_new ("");
    if ((ghost_pad = gst_element_get_static_pad (bin, "sink"))) {
      g_assert (GST_IS_GHOST_PAD (ghost_pad));

      target_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (ghost_pad));
      g_assert (target_pad != NULL);
      g_assert (GST_IS_PAD (target_pad));

      parent = gst_pad_get_parent_element (target_pad);
      g_assert (parent != NULL);

      g_string_append_printf (s, "%s/sink", GST_ELEMENT_NAME (parent));

      gst_object_unref (parent);
      gst_object_unref (target_pad);
      gst_object_unref (ghost_pad);
    }

    if ((ghost_pad = gst_element_get_static_pad (bin, "src"))) {
      g_assert (GST_IS_GHOST_PAD (ghost_pad));

      target_pad = gst_ghost_pad_get_target (GST_GHOST_PAD (ghost_pad));
      g_assert (target_pad != NULL);
      g_assert (GST_IS_PAD (target_pad));

      parent = gst_pad_get_parent_element (target_pad);
      g_assert (parent != NULL);

      if (s->len > 0) {
        g_string_append (s, ",");
      }

      g_string_append_printf (s, "%s/src", GST_ELEMENT_NAME (parent));

      gst_object_unref (parent);
      gst_object_unref (target_pad);
      gst_object_unref (ghost_pad);
    }

    if (strcmp (s->str, bin_tests[i].pad_names) != 0) {
      g_error ("FAILED: expected '%s', got '%s' for bin '%s'",
          bin_tests[i].pad_names, s->str, bin_tests[i].bin_desc);
    }
    g_string_free (s, TRUE);

    gst_object_unref (bin);
  }
}

GST_END_TEST;
#endif

GST_START_TEST (test_element_found_tags)
{
  GstElement *pipeline, *fakesrc, *fakesink;
  GstTagList *list;
  GstBus *bus;
  GstMessage *message;
  GstPad *srcpad;

  pipeline = gst_element_factory_make ("pipeline", NULL);
  fakesrc = gst_element_factory_make ("fakesrc", NULL);
  fakesink = gst_element_factory_make ("fakesink", NULL);
  list = gst_tag_list_new_empty ();

  g_object_set (fakesrc, "num-buffers", (int) 10, NULL);

  gst_bin_add_many (GST_BIN (pipeline), fakesrc, fakesink, NULL);
  gst_element_link (fakesrc, fakesink);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  srcpad = gst_element_get_static_pad (fakesrc, "src");
  gst_pad_push_event (srcpad, gst_event_new_tag (list));
  gst_object_unref (srcpad);

  bus = gst_element_get_bus (pipeline);
  message = gst_bus_poll (bus, GST_MESSAGE_EOS, -1);
  gst_message_unref (message);
  gst_object_unref (bus);

  /* FIXME: maybe also check if the fakesink receives the message */

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
}

GST_END_TEST;

typedef struct _GstLinker GstLinker;
typedef struct _GstLinkerClass GstLinkerClass;

#define GST_TYPE_LINKER                (gst_linker_get_type ())
#define GST_IS_LINKER(obj)             (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_LINKER))
#define GST_IS_LINKER_CLASS(klass)     (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_LINKER))
#define GST_LINKER_GET_CLASS(obj)      (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_LINKER, GstLinkerClass))
#define GST_LINKER(obj)                (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_LINKER, GstLinker))
#define GST_LINKER_CLASS(klass)        (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_LINKER, GstLinkerClass))
#define GST_LINKER_CAST(obj)           ((GstLinker*)(obj))

struct _GstLinker
{
  GstElement element;

  GstPad *sinkpad0;
  GstPad *sinkpad1;
  GstPad *srcpad0;
  GstPad *srcpad1;

  gchar *sinkpad0_config;
  gchar *sinkpad1_config;
  gchar *srcpad0_config;
  gchar *srcpad1_config;
};

struct _GstLinkerClass
{
  GstElementClass parent_class;
};

enum
{
  PROP_0,
  PROP_SINKPAD0,
  PROP_SINKPAD1,
  PROP_SRCPAD0,
  PROP_SRCPAD1,
  PROP_LAST
};


G_GNUC_INTERNAL GType gst_linker_get_type (void);

static GstStaticPadTemplate linker_sinkpad_template =
GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

static GstStaticPadTemplate linker_srcpad_template =
GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS_ANY);

#define gst_linker_parent_class parent_class
G_DEFINE_TYPE (GstLinker, gst_linker, GST_TYPE_ELEMENT);

static GstPadLinkReturn
gst_linker_refuse_to_link (GstPad * pad, GstObject * parent, GstPad * peer)
{
  return GST_PAD_LINK_REFUSED;
}

static GstPad *
gst_linker_request_new_pad (GstElement * element, GstPadTemplate * templ,
    const gchar * name, const GstCaps * caps)
{
  GstLinker *linker = GST_LINKER (element);
  const gchar *templ_name;
  const gchar *sinkpad_name;
  const gchar *srcpad_name;
  GstPad **newpad;
  gchar *config = NULL;
  gchar *pad_name;

  GST_INFO_OBJECT (element,
      "Requesting new pad \"%s\" using template %" GST_PTR_FORMAT, name, templ);

  fail_unless (GST_IS_PAD_TEMPLATE (templ));

  templ_name = GST_PAD_TEMPLATE_NAME_TEMPLATE (templ);
  sinkpad_name = linker_sinkpad_template.name_template;
  srcpad_name = linker_srcpad_template.name_template;

  if (g_strcmp0 (templ_name, sinkpad_name) == 0) {
    if (linker->sinkpad0 != NULL && linker->sinkpad1 != NULL) {
      GST_INFO_OBJECT (element, "Denied, too many sinkpads");
      return NULL;
    }
    if (g_strcmp0 (name, "src_0") == 0) {
      newpad = &(linker->srcpad0);
      config = linker->srcpad0_config;
      GST_INFO_OBJECT (element, "No srcpads present, this is port 0");
      name = "src_0";
    } else if (g_strcmp0 (name, "src_1") == 0) {
      newpad = &(linker->srcpad1);
      config = linker->srcpad1_config;
      GST_INFO_OBJECT (element, "One srcpad present, this is port 1");
      name = "src_1";
    } else if (linker->sinkpad0 == NULL) {
      newpad = &(linker->sinkpad0);
      config = linker->sinkpad0_config;
      GST_INFO_OBJECT (element, "No sinkpads present, this is port 0");
      name = "sink_0";
    } else {
      newpad = &(linker->sinkpad1);
      config = linker->sinkpad1_config;
      GST_INFO_OBJECT (element, "One sinkpad present, this is port 1");
      name = "sink_1";
    }
  } else if (g_strcmp0 (templ_name, srcpad_name) == 0) {
    if (linker->srcpad0 != NULL && linker->srcpad1 != NULL) {
      GST_INFO_OBJECT (element, "Denied, too many srcpads");
      return NULL;
    }
    if (g_strcmp0 (name, "src_0") == 0) {
      newpad = &(linker->srcpad0);
      config = linker->srcpad0_config;
      GST_INFO_OBJECT (element, "No srcpads present, this is port 0");
      name = "src_0";
    } else if (g_strcmp0 (name, "src_1") == 0) {
      newpad = &(linker->srcpad1);
      config = linker->srcpad1_config;
      GST_INFO_OBJECT (element, "One srcpad present, this is port 1");
      name = "src_1";
    } else if (linker->srcpad0 == NULL) {
      newpad = &(linker->srcpad0);
      config = linker->srcpad0_config;
      GST_INFO_OBJECT (element, "No srcpads present, this is port 0");
      name = "src_0";
    } else {
      newpad = &(linker->srcpad1);
      config = linker->srcpad1_config;
      GST_INFO_OBJECT (element, "One srcpad present, this is port 1");
      name = "src_1";
    }
  } else {
    g_assert_not_reached ();
  }

  GST_INFO_OBJECT (element, "Pad configuration: %s", config);

  if (g_strstr_len (config, -1, "present") == NULL) {
    GST_INFO_OBJECT (element, "Denied, port is not present");
    return NULL;
  }

  *newpad = gst_pad_new_from_template (templ, name);
  gst_element_add_pad (element, *newpad);
  pad_name = gst_pad_get_name (*newpad);
  GST_WARNING_OBJECT (element, "Created new request pad \"%s\"", pad_name);

  if (g_strstr_len (config, -1, "unlinkable") != NULL) {
    gst_pad_set_link_function (*newpad, gst_linker_refuse_to_link);
  } else if (g_strstr_len (config, -1, "unlinked") == NULL) {
    if (GST_PAD_TEMPLATE_DIRECTION (templ) == GST_PAD_SINK) {
      GstElement *fakesrc = gst_element_factory_make ("fakesrc", NULL);
      g_assert (gst_element_link_pads (fakesrc, NULL, element, pad_name));
    } else if (GST_PAD_TEMPLATE_DIRECTION (templ) == GST_PAD_SRC) {
      GstElement *fakesink = gst_element_factory_make ("fakesink", NULL);
      g_assert (gst_element_link_pads (element, pad_name, fakesink, NULL));
    } else {
      g_assert_not_reached ();
    }
  }

  g_free (pad_name);

  return *newpad;
}

static void
gst_linker_release_pad (GstElement * element, GstPad * pad)
{
  GstLinker *linker = GST_LINKER (element);
  gchar *pad_name = gst_pad_get_name (pad);
  GST_WARNING_OBJECT (element, "Releasing request pad \"%s\"", pad_name);
  g_free (pad_name);
  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (element, pad);

  if (linker->sinkpad0 == pad)
    linker->sinkpad0 = NULL;
  if (linker->sinkpad1 == pad)
    linker->sinkpad1 = NULL;
  if (linker->srcpad0 == pad)
    linker->srcpad0 = NULL;
  if (linker->srcpad1 == pad)
    linker->srcpad1 = NULL;
}

static void
gst_linker_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstLinker *linker = GST_LINKER (object);

  GST_OBJECT_LOCK (linker);
  switch (prop_id) {
    case PROP_SINKPAD0:
      g_value_set_string (value, linker->sinkpad0_config);
      break;
    case PROP_SINKPAD1:
      g_value_set_string (value, linker->sinkpad1_config);
      break;
    case PROP_SRCPAD0:
      g_value_set_string (value, linker->srcpad0_config);
      break;
    case PROP_SRCPAD1:
      g_value_set_string (value, linker->srcpad1_config);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (linker);
}

static void
gst_linker_dump_state (GstLinker * linker)
{
  GST_ERROR_OBJECT (linker, "sinkpad0(%sallocated): %s",
      linker->sinkpad0 ? "" : "un", linker->sinkpad0_config);
  GST_ERROR_OBJECT (linker, "sinkpad1(%sallocated): %s",
      linker->sinkpad1 ? "" : "un", linker->sinkpad1_config);
  GST_ERROR_OBJECT (linker, "srcpad0(%sallocated): %s",
      linker->srcpad0 ? "" : "un", linker->srcpad0_config);
  GST_ERROR_OBJECT (linker, "srcpad1(%sallocated): %s",
      linker->srcpad1 ? "" : "un", linker->srcpad1_config);
}

static void
gst_linker_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstLinker *linker = GST_LINKER (object);

  GST_OBJECT_LOCK (linker);
  switch (prop_id) {
    case PROP_SINKPAD0:
      if (linker->sinkpad0 == NULL) {
        linker->sinkpad0_config = g_value_dup_string (value);
        GST_INFO_OBJECT (linker, "sinkpad0: %s", linker->sinkpad0_config);
      } else {
        GST_ERROR_OBJECT (linker, "unable to reconfigure existing sinkpad0");
        gst_linker_dump_state (linker);
        abort ();
      }
      break;
    case PROP_SINKPAD1:
      if (linker->sinkpad1 == NULL) {
        linker->sinkpad1_config = g_value_dup_string (value);
        GST_INFO_OBJECT (linker, "sinkpad1: %s", linker->sinkpad1_config);
      } else {
        GST_ERROR_OBJECT (linker, "unable to reconfigure existing sinkpad1");
        gst_linker_dump_state (linker);
        abort ();
      }
      break;
    case PROP_SRCPAD0:
      if (linker->srcpad0 == NULL) {
        linker->srcpad0_config = g_value_dup_string (value);
        GST_INFO_OBJECT (linker, "srcpad0: %s", linker->srcpad0_config);
      } else {
        GST_ERROR_OBJECT (linker, "unable to reconfigure existing srcpad0");
        gst_linker_dump_state (linker);
        abort ();
      }
      break;
    case PROP_SRCPAD1:
      if (linker->srcpad1 == NULL) {
        linker->srcpad1_config = g_value_dup_string (value);
        GST_INFO_OBJECT (linker, "srcpad1: %s", linker->srcpad1_config);
      } else {
        GST_ERROR_OBJECT (linker, "unable to reconfigure existing srcpad1");
        gst_linker_dump_state (linker);
        abort ();
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
  GST_OBJECT_UNLOCK (linker);
}

static void
gst_linker_iterate_pads (GstElement * linker, GstIteratorForeachFunction func,
    gpointer user_data)
{
  GstIterator *iter;
  GstIteratorResult result;

  iter = gst_element_iterate_pads (linker);
  do {
    result = gst_iterator_foreach (iter, func, user_data);
    if (result == GST_ITERATOR_RESYNC) {
      gst_iterator_resync (iter);
    }
  } while (result == GST_ITERATOR_RESYNC);
  gst_iterator_free (iter);
}

static void
gst_linker_unlink_request_pad (const GValue * item, gpointer user_data)
{
  GstElement *linker = GST_ELEMENT (user_data);
  GstPad *pad = g_value_get_object (item);
  GstPadTemplate *templ = gst_pad_get_pad_template (pad);

  if (GST_PAD_TEMPLATE_PRESENCE (templ) == GST_PAD_REQUEST) {
    gst_element_release_request_pad (linker, pad);
  }

  gst_object_unref (templ);
}

static void
gst_linker_release_request_pads (GstElement * linker)
{
  gst_linker_iterate_pads (linker, gst_linker_unlink_request_pad, linker);
}

static void
gst_linker_count_request_pad (const GValue * item, gpointer user_data)
{
  guint *counter = (guint *) user_data;
  GstPad *pad = g_value_get_object (item);
  GstPadTemplate *templ = gst_pad_get_pad_template (pad);

  if (GST_PAD_TEMPLATE_PRESENCE (templ) == GST_PAD_REQUEST) {
    (*counter)++;
  }

  gst_object_unref (templ);
}

static guint
gst_linker_count_request_pads (GstElement * linker)
{
  guint counter = 0;
  gst_linker_iterate_pads (linker, gst_linker_count_request_pad, &counter);
  return counter;
}

static void
gst_linker_unref_linked_pad_peer (const GValue * item, gpointer user_data)
{
  GstElement *linker = GST_ELEMENT (user_data);
  GstPad *pad = g_value_get_object (item);
  GstPad *otherpad = gst_pad_get_peer (pad);
  GstElement *otherelement = gst_pad_get_parent_element (otherpad);
  gst_element_unlink (linker, otherelement);
  gst_object_unref (otherelement);
  gst_object_unref (otherpad);
}

static void
gst_linker_unref_linked_pads_peer (GstElement * linker)
{
  gst_linker_iterate_pads (linker, gst_linker_unref_linked_pad_peer, linker);
}

static void
gst_linker_dispose (GObject * object)
{
  GstLinker *linker = GST_LINKER (object);

  g_free (linker->sinkpad0_config);
  g_free (linker->sinkpad1_config);
  g_free (linker->srcpad0_config);
  g_free (linker->srcpad1_config);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_linker_class_init (GstLinkerClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  gobject_class->dispose = gst_linker_dispose;
  gobject_class->get_property = gst_linker_get_property;
  gobject_class->set_property = gst_linker_set_property;

  g_object_class_install_property (gobject_class, PROP_SINKPAD0,
      g_param_spec_string ("sinkpad0", "Sink pad 0",
          "Configuration of sinkpad0", "missing",
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SINKPAD1,
      g_param_spec_string ("sinkpad1", "Sink pad 1",
          "Configuration of sinkpad1", "missing",
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SRCPAD0,
      g_param_spec_string ("srcpad0", "Source pad 0",
          "Configuration of srcpad0", "missing",
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class, PROP_SRCPAD1,
      g_param_spec_string ("srcpad1", "Source pad 1",
          "Configuration of srcpad1", "missing",
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (gstelement_class,
      "Linker", "Linking element", "Element used for testing linking",
      "Sebastian Rasmussen <sebras@hotmail.com>");

  gstelement_class->request_new_pad = gst_linker_request_new_pad;
  gstelement_class->release_pad = gst_linker_release_pad;

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&linker_sinkpad_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&linker_srcpad_template));
}

static void
gst_linker_init (GstLinker * linker)
{
}


typedef enum
{
  PAD_PRESENCE_MASK = 0x1,
  PAD_LINKABILITY_MASK = 0x2,
  PAD_PRELINKING_MASK = 0x4,
  PAD_COMBINATIONS = 0x8,
  PAD_UNSPECIFIED,
} PadConfig;

static gchar *
build_pad_config (PadConfig config)
{
  GString *str;

  if (config == PAD_UNSPECIFIED)
    return g_strdup ("");

  str = g_string_new (NULL);

  if (config & PAD_PRESENCE_MASK)
    g_string_append (str, "present");
  else
    g_string_append (str, "missing");

  if (config & PAD_LINKABILITY_MASK)
    g_string_append (str, ",linkable");
  else
    g_string_append (str, ",unlinkable");

  if (config & PAD_PRELINKING_MASK)
    g_string_append (str, ",linked");
  else
    g_string_append (str, ",unlinked");

  return g_string_free (str, FALSE);
}

static gchar *
build_padname (const gchar * name_prefix, const gchar * name_suffix)
{
  if (name_suffix == NULL) {
    return NULL;
  } else {
    GString *name = g_string_new (NULL);
    if (name_suffix != NULL)
      g_string_append_printf (name, "%s_%s", name_prefix, name_suffix);
    return g_string_free (name, FALSE);
  }
}

static void
here (void)
{
  GST_ERROR ("HERE");
}

GST_START_TEST (test_element_link)
{
  const gchar *padnames[] = {
    NULL,
    "0",
    "1",
  };
  const guint successes[] = {
    G_MAXUINT,
  };
  guint srcsrc0, srcsrc1;
  guint sinksink0, sinksink1;
  guint srcpad, sinkpad, idx;
  GstElement *src, *sink;

  src = g_object_new (GST_TYPE_LINKER, "name", "src", NULL);
  sink = g_object_new (GST_TYPE_LINKER, "name", "sink", NULL);

  idx = 0;
  for (srcsrc0 = 0; srcsrc0 < PAD_COMBINATIONS; srcsrc0++) {
    for (srcsrc1 = 0; srcsrc1 < PAD_COMBINATIONS; srcsrc1++) {
      for (sinksink0 = 0; sinksink0 < PAD_COMBINATIONS; sinksink0++) {
        for (sinksink1 = 0; sinksink1 < PAD_COMBINATIONS; sinksink1++) {
          for (srcpad = 0; srcpad < G_N_ELEMENTS (padnames); srcpad++) {
            for (sinkpad = 0; sinkpad < G_N_ELEMENTS (padnames); sinkpad++) {
              gchar *srcsrc0_config, *srcsrc1_config;
              gchar *sinksink0_config, *sinksink1_config;
              gchar *src_padname, *sink_padname;
              gboolean expectation = FALSE;
              guint i;

              if ((idx % 10000) == 0)
                GST_ERROR ("Test: %4u", idx);

              if (!((srcsrc0 | srcsrc1) & PAD_PRESENCE_MASK)) {
                GST_ERROR ("skipping %u because src linker has no pads", idx);
                idx++;
                continue;
              }
              if (!((sinksink0 | sinksink1) & PAD_PRESENCE_MASK)) {
                GST_ERROR ("skipping %u because sink linker has no pads", idx);
                idx++;
                continue;
              }

              srcsrc0_config = build_pad_config (srcsrc0);
              srcsrc1_config = build_pad_config (srcsrc1);
              sinksink0_config = build_pad_config (sinksink0);
              sinksink1_config = build_pad_config (sinksink1);
              src_padname = build_padname ("src", padnames[srcpad]);
              sink_padname = build_padname ("sink", padnames[sinkpad]);

              GST_WARNING ("Test: %4u", idx);
              GST_WARNING ("srcsrc0: %s", srcsrc0_config);
              GST_WARNING ("srcsrc1: %s", srcsrc1_config);
              GST_WARNING ("sinksink0: %s", sinksink0_config);
              GST_WARNING ("sinksink1: %s", sinksink1_config);
              GST_WARNING ("pads: %s<->%s", src_padname, sink_padname);

              for (i = 0; i < G_N_ELEMENTS (successes); i++)
                if (successes[i] == idx)
                  expectation = TRUE;

              g_object_set (src, "srcpad0", srcsrc0_config, "srcpad1",
                  srcsrc1_config, NULL);

              g_object_set (sink, "sinkpad0", sinksink0_config, "sinkpad1",
                  sinksink1_config, NULL);

              g_free (srcsrc0_config);
              g_free (srcsrc1_config);
              g_free (sinksink0_config);
              g_free (sinksink1_config);

              if (idx == atoi (getenv ("IDX"))) {
                here ();
              }

              if (gst_element_link_pads (src, src_padname, sink, sink_padname)) {
                GST_ERROR ("successful: %u", idx);
                gst_element_unlink (src, sink);
                gst_linker_release_request_pads (src);
                gst_linker_release_request_pads (sink);
                if (expectation)
                  GST_DEBUG ("MU");
              }

              gst_linker_unref_linked_pads_peer (src);
              gst_linker_unref_linked_pads_peer (sink);

              fail_unless (gst_linker_count_request_pads (src) == 0);
              fail_unless (gst_linker_count_request_pads (sink) == 0);

              g_free (src_padname);
              g_free (sink_padname);

              idx++;
            }
          }
        }
      }
    }
  }

  gst_object_unref (src);
  gst_object_unref (sink);
}

GST_END_TEST;


GST_START_TEST (test_element_unlink)
{
  GstElement *src, *sink;

  src = gst_element_factory_make ("fakesrc", NULL);
  sink = gst_element_factory_make ("fakesink", NULL);
  fail_unless (gst_element_link (src, sink) != FALSE);
  gst_element_unlink (src, sink);
  gst_object_unref (src);
  gst_object_unref (sink);
}

GST_END_TEST;

GST_START_TEST (test_set_value_from_string)
{
  GValue val = { 0, };

  /* g_return_if_fail */
  ASSERT_CRITICAL (gst_util_set_value_from_string (NULL, "xyz"));

  g_value_init (&val, G_TYPE_STRING);
  ASSERT_CRITICAL (gst_util_set_value_from_string (&val, NULL));
  g_value_unset (&val);

  /* string => string */
  g_value_init (&val, G_TYPE_STRING);
  gst_util_set_value_from_string (&val, "Y00");
  fail_unless (g_value_get_string (&val) != NULL);
  fail_unless_equals_string (g_value_get_string (&val), "Y00");
  g_value_unset (&val);

  /* string => int */
  g_value_init (&val, G_TYPE_INT);
  gst_util_set_value_from_string (&val, "987654321");
  fail_unless (g_value_get_int (&val) == 987654321);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_INT);
  ASSERT_CRITICAL (gst_util_set_value_from_string (&val, "xyz"));
  g_value_unset (&val);

  /* string => uint */
  g_value_init (&val, G_TYPE_UINT);
  gst_util_set_value_from_string (&val, "987654321");
  fail_unless (g_value_get_uint (&val) == 987654321);
  g_value_unset (&val);

  /* CHECKME: is this really desired behaviour? (tpm) */
  g_value_init (&val, G_TYPE_UINT);
  gst_util_set_value_from_string (&val, "-999");
  fail_unless (g_value_get_uint (&val) == ((guint) 0 - (guint) 999));
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_UINT);
  ASSERT_CRITICAL (gst_util_set_value_from_string (&val, "xyz"));
  g_value_unset (&val);

  /* string => long */
  g_value_init (&val, G_TYPE_LONG);
  gst_util_set_value_from_string (&val, "987654321");
  fail_unless (g_value_get_long (&val) == 987654321);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_LONG);
  ASSERT_CRITICAL (gst_util_set_value_from_string (&val, "xyz"));
  g_value_unset (&val);

  /* string => ulong */
  g_value_init (&val, G_TYPE_ULONG);
  gst_util_set_value_from_string (&val, "987654321");
  fail_unless (g_value_get_ulong (&val) == 987654321);
  g_value_unset (&val);

  /* CHECKME: is this really desired behaviour? (tpm) */
  g_value_init (&val, G_TYPE_ULONG);
  gst_util_set_value_from_string (&val, "-999");
  fail_unless (g_value_get_ulong (&val) == ((gulong) 0 - (gulong) 999));
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_ULONG);
  ASSERT_CRITICAL (gst_util_set_value_from_string (&val, "xyz"));
  g_value_unset (&val);

  /* string => boolean */
  g_value_init (&val, G_TYPE_BOOLEAN);
  gst_util_set_value_from_string (&val, "true");
  fail_unless_equals_int (g_value_get_boolean (&val), TRUE);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_BOOLEAN);
  gst_util_set_value_from_string (&val, "TRUE");
  fail_unless_equals_int (g_value_get_boolean (&val), TRUE);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_BOOLEAN);
  gst_util_set_value_from_string (&val, "false");
  fail_unless_equals_int (g_value_get_boolean (&val), FALSE);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_BOOLEAN);
  gst_util_set_value_from_string (&val, "FALSE");
  fail_unless_equals_int (g_value_get_boolean (&val), FALSE);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_BOOLEAN);
  gst_util_set_value_from_string (&val, "bleh");
  fail_unless_equals_int (g_value_get_boolean (&val), FALSE);
  g_value_unset (&val);

#if 0
  /* string => float (yay, localisation issues involved) */
  g_value_init (&val, G_TYPE_FLOAT);
  gst_util_set_value_from_string (&val, "987.654");
  fail_unless (g_value_get_float (&val) >= 987.653 &&
      g_value_get_float (&val) <= 987.655);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_FLOAT);
  gst_util_set_value_from_string (&val, "987,654");
  fail_unless (g_value_get_float (&val) >= 987.653 &&
      g_value_get_float (&val) <= 987.655);
  g_value_unset (&val);

  /* string => double (yay, localisation issues involved) */
  g_value_init (&val, G_TYPE_DOUBLE);
  gst_util_set_value_from_string (&val, "987.654");
  fail_unless (g_value_get_double (&val) >= 987.653 &&
      g_value_get_double (&val) <= 987.655);
  g_value_unset (&val);

  g_value_init (&val, G_TYPE_DOUBLE);
  gst_util_set_value_from_string (&val, "987,654");
  fail_unless (g_value_get_double (&val) >= 987.653 &&
      g_value_get_double (&val) <= 987.655);
  g_value_unset (&val);
#endif
}

GST_END_TEST;

static gint
_binary_search_compare (guint32 * a, guint32 * b)
{
  return *a - *b;
}

GST_START_TEST (test_binary_search)
{
  guint32 data[257];
  guint32 *match;
  guint32 search_element = 121 * 2;
  guint i;

  for (i = 0; i < 257; i++)
    data[i] = (i + 1) * 2;

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_EXACT,
      &search_element, NULL);
  fail_unless (match != NULL);
  fail_unless_equals_int (match - data, 120);

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_BEFORE,
      &search_element, NULL);
  fail_unless (match != NULL);
  fail_unless_equals_int (match - data, 120);

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_AFTER,
      &search_element, NULL);
  fail_unless (match != NULL);
  fail_unless_equals_int (match - data, 120);

  search_element = 0;
  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_EXACT,
      &search_element, NULL);
  fail_unless (match == NULL);

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_AFTER,
      &search_element, NULL);
  fail_unless (match != NULL);
  fail_unless_equals_int (match - data, 0);

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_BEFORE,
      &search_element, NULL);
  fail_unless (match == NULL);

  search_element = 1000;
  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_EXACT,
      &search_element, NULL);
  fail_unless (match == NULL);

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_AFTER,
      &search_element, NULL);
  fail_unless (match == NULL);

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_BEFORE,
      &search_element, NULL);
  fail_unless (match != NULL);
  fail_unless_equals_int (match - data, 256);

  search_element = 121 * 2 - 1;
  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_EXACT,
      &search_element, NULL);
  fail_unless (match == NULL);

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_AFTER,
      &search_element, NULL);
  fail_unless (match != NULL);
  fail_unless_equals_int (match - data, 120);

  match =
      (guint32 *) gst_util_array_binary_search (data, 257, sizeof (guint32),
      (GCompareDataFunc) _binary_search_compare, GST_SEARCH_MODE_BEFORE,
      &search_element, NULL);
  fail_unless (match != NULL);
  fail_unless_equals_int (match - data, 119);

}

GST_END_TEST;

#ifdef HAVE_GSL
#ifdef HAVE_GMP

#include <gsl/gsl_rng.h>
#include <gmp.h>

static guint64
randguint64 (gsl_rng * rng, guint64 n)
{
  union
  {
    guint64 x;
    struct
    {
      guint16 a, b, c, d;
    } parts;
  } x;
  x.parts.a = gsl_rng_uniform_int (rng, 1 << 16);
  x.parts.b = gsl_rng_uniform_int (rng, 1 << 16);
  x.parts.c = gsl_rng_uniform_int (rng, 1 << 16);
  x.parts.d = gsl_rng_uniform_int (rng, 1 << 16);
  return x.x % n;
}


enum round_t
{
  ROUND_TONEAREST = 0,
  ROUND_UP,
  ROUND_DOWN
};

static void
gmp_set_uint64 (mpz_t mp, guint64 x)
{
  mpz_t two_32, tmp;

  mpz_init (two_32);
  mpz_init (tmp);

  mpz_ui_pow_ui (two_32, 2, 32);
  mpz_set_ui (mp, (unsigned long) ((x >> 32) & G_MAXUINT32));
  mpz_mul (tmp, mp, two_32);
  mpz_add_ui (mp, tmp, (unsigned long) (x & G_MAXUINT32));
  mpz_clear (two_32);
  mpz_clear (tmp);
}

static guint64
gmp_get_uint64 (mpz_t mp)
{
  mpz_t two_64, two_32, tmp;
  guint64 ret;

  mpz_init (two_64);
  mpz_init (two_32);
  mpz_init (tmp);

  mpz_ui_pow_ui (two_64, 2, 64);
  mpz_ui_pow_ui (two_32, 2, 32);
  if (mpz_cmp (tmp, two_64) >= 0)
    return G_MAXUINT64;
  mpz_clear (two_64);

  mpz_tdiv_q (tmp, mp, two_32);
  ret = mpz_get_ui (tmp);
  ret <<= 32;
  ret |= mpz_get_ui (mp);
  mpz_clear (two_32);
  mpz_clear (tmp);

  return ret;
}

static guint64
gmp_scale (guint64 x, guint64 a, guint64 b, enum round_t mode)
{
  mpz_t mp1, mp2, mp3;
  if (!b)
    /* overflow */
    return G_MAXUINT64;
  mpz_init (mp1);
  mpz_init (mp2);
  mpz_init (mp3);

  gmp_set_uint64 (mp1, x);
  gmp_set_uint64 (mp3, a);
  mpz_mul (mp2, mp1, mp3);
  switch (mode) {
    case ROUND_TONEAREST:
      gmp_set_uint64 (mp1, b);
      mpz_tdiv_q_ui (mp3, mp1, 2);
      mpz_add (mp1, mp2, mp3);
      mpz_set (mp2, mp1);
      break;
    case ROUND_UP:
      gmp_set_uint64 (mp1, b);
      mpz_sub_ui (mp3, mp1, 1);
      mpz_add (mp1, mp2, mp3);
      mpz_set (mp2, mp1);
      break;
    case ROUND_DOWN:
      break;
  }
  gmp_set_uint64 (mp3, b);
  mpz_tdiv_q (mp1, mp2, mp3);
  x = gmp_get_uint64 (mp1);
  mpz_clear (mp1);
  mpz_clear (mp2);
  mpz_clear (mp3);
  return x;
}

static void
_gmp_test_scale (gsl_rng * rng)
{
  guint64 bygst, bygmp;
  guint64 a = randguint64 (rng, gsl_rng_uniform_int (rng,
          2) ? G_MAXUINT64 : G_MAXUINT32);
  guint64 b = randguint64 (rng, gsl_rng_uniform_int (rng, 2) ? G_MAXUINT64 - 1 : G_MAXUINT32 - 1) + 1;  /* 0 not allowed */
  guint64 val = randguint64 (rng, gmp_scale (G_MAXUINT64, b, a, ROUND_DOWN));
  enum round_t mode = gsl_rng_uniform_int (rng, 3);
  const char *func;

  bygmp = gmp_scale (val, a, b, mode);
  switch (mode) {
    case ROUND_TONEAREST:
      bygst = gst_util_uint64_scale_round (val, a, b);
      func = "gst_util_uint64_scale_round";
      break;
    case ROUND_UP:
      bygst = gst_util_uint64_scale_ceil (val, a, b);
      func = "gst_util_uint64_scale_ceil";
      break;
    case ROUND_DOWN:
      bygst = gst_util_uint64_scale (val, a, b);
      func = "gst_util_uint64_scale";
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  fail_unless (bygst == bygmp,
      "error: %s(): %" G_GUINT64_FORMAT " * %" G_GUINT64_FORMAT " / %"
      G_GUINT64_FORMAT " = %" G_GUINT64_FORMAT ", correct = %" G_GUINT64_FORMAT
      "\n", func, val, a, b, bygst, bygmp);
}

static void
_gmp_test_scale_int (gsl_rng * rng)
{
  guint64 bygst, bygmp;
  gint32 a = randguint64 (rng, G_MAXINT32);
  gint32 b = randguint64 (rng, G_MAXINT32 - 1) + 1;     /* 0 not allowed */
  guint64 val = randguint64 (rng, gmp_scale (G_MAXUINT64, b, a, ROUND_DOWN));
  enum round_t mode = gsl_rng_uniform_int (rng, 3);
  const char *func;

  bygmp = gmp_scale (val, a, b, mode);
  switch (mode) {
    case ROUND_TONEAREST:
      bygst = gst_util_uint64_scale_int_round (val, a, b);
      func = "gst_util_uint64_scale_int_round";
      break;
    case ROUND_UP:
      bygst = gst_util_uint64_scale_int_ceil (val, a, b);
      func = "gst_util_uint64_scale_int_ceil";
      break;
    case ROUND_DOWN:
      bygst = gst_util_uint64_scale_int (val, a, b);
      func = "gst_util_uint64_scale_int";
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  fail_unless (bygst == bygmp,
      "error: %s(): %" G_GUINT64_FORMAT " * %d / %d = %" G_GUINT64_FORMAT
      ", correct = %" G_GUINT64_FORMAT "\n", func, val, a, b, bygst, bygmp);
}

#define GMP_TEST_RUNS 100000

GST_START_TEST (test_math_scale_gmp)
{
  gsl_rng *rng = gsl_rng_alloc (gsl_rng_mt19937);
  gint n;

  for (n = 0; n < GMP_TEST_RUNS; n++)
    _gmp_test_scale (rng);

  gsl_rng_free (rng);
}

GST_END_TEST;

GST_START_TEST (test_math_scale_gmp_int)
{
  gsl_rng *rng = gsl_rng_alloc (gsl_rng_mt19937);
  gint n;

  for (n = 0; n < GMP_TEST_RUNS; n++)
    _gmp_test_scale_int (rng);

  gsl_rng_free (rng);
}

GST_END_TEST;

#endif
#endif

GST_START_TEST (test_pad_proxy_query_caps_aggregation)
{
  GstElement *tee, *sink1, *sink2;
  GstCaps *caps;
  GstPad *tee_src1, *tee_src2, *tee_sink, *sink1_sink, *sink2_sink;

  tee = gst_element_factory_make ("tee", "tee");

  sink1 = gst_element_factory_make ("fakesink", "sink1");
  tee_src1 = gst_element_get_request_pad (tee, "src_%u");
  sink1_sink = gst_element_get_static_pad (sink1, "sink");
  fail_unless_equals_int (gst_pad_link (tee_src1, sink1_sink), GST_PAD_LINK_OK);

  sink2 = gst_element_factory_make ("fakesink", "sink2");
  tee_src2 = gst_element_get_request_pad (tee, "src_%u");
  sink2_sink = gst_element_get_static_pad (sink2, "sink");
  fail_unless_equals_int (gst_pad_link (tee_src2, sink2_sink), GST_PAD_LINK_OK);

  tee_sink = gst_element_get_static_pad (tee, "sink");

  gst_element_set_state (sink1, GST_STATE_PAUSED);
  gst_element_set_state (sink2, GST_STATE_PAUSED);
  gst_element_set_state (tee, GST_STATE_PAUSED);

  /* by default, ANY caps should intersect to ANY */
  caps = gst_pad_query_caps (tee_sink, NULL);
  GST_INFO ("got caps: %" GST_PTR_FORMAT, caps);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_is_any (caps));
  gst_caps_unref (caps);

  /* these don't intersect we should get empty caps */
  caps = gst_caps_new_empty_simple ("foo/bar");
  fail_unless (gst_pad_set_caps (sink1_sink, caps));
  gst_pad_use_fixed_caps (sink1_sink);
  gst_caps_unref (caps);

  caps = gst_caps_new_empty_simple ("bar/ter");
  fail_unless (gst_pad_set_caps (sink2_sink, caps));
  gst_pad_use_fixed_caps (sink2_sink);
  gst_caps_unref (caps);

  caps = gst_pad_query_caps (tee_sink, NULL);
  GST_INFO ("got caps: %" GST_PTR_FORMAT, caps);
  fail_unless (caps != NULL);
  fail_unless (gst_caps_is_empty (caps));
  gst_caps_unref (caps);

  /* test intersection */
  caps = gst_caps_new_simple ("foo/bar", "barversion", G_TYPE_INT, 1, NULL);
  GST_OBJECT_FLAG_UNSET (sink2_sink, GST_PAD_FLAG_FIXED_CAPS);
  fail_unless (gst_pad_set_caps (sink2_sink, caps));
  gst_pad_use_fixed_caps (sink2_sink);
  gst_caps_unref (caps);

  caps = gst_pad_query_caps (tee_sink, NULL);
  GST_INFO ("got caps: %" GST_PTR_FORMAT, caps);
  fail_unless (caps != NULL);
  fail_if (gst_caps_is_empty (caps));
  {
    GstStructure *s = gst_caps_get_structure (caps, 0);

    fail_unless_equals_string (gst_structure_get_name (s), "foo/bar");
    fail_unless (gst_structure_has_field_typed (s, "barversion", G_TYPE_INT));
  }
  gst_caps_unref (caps);

  gst_element_set_state (sink1, GST_STATE_NULL);
  gst_element_set_state (sink2, GST_STATE_NULL);
  gst_element_set_state (tee, GST_STATE_NULL);

  /* clean up */
  gst_element_release_request_pad (tee, tee_src1);
  gst_object_unref (tee_src1);
  gst_element_release_request_pad (tee, tee_src2);
  gst_object_unref (tee_src2);
  gst_object_unref (tee_sink);
  gst_object_unref (tee);
  gst_object_unref (sink1_sink);
  gst_object_unref (sink1);
  gst_object_unref (sink2_sink);
  gst_object_unref (sink2);
}

GST_END_TEST;

GST_START_TEST (test_greatest_common_divisor)
{
  fail_if (gst_util_greatest_common_divisor (1, 1) != 1);
  fail_if (gst_util_greatest_common_divisor (2, 3) != 1);
  fail_if (gst_util_greatest_common_divisor (3, 5) != 1);
  fail_if (gst_util_greatest_common_divisor (-1, 1) != 1);
  fail_if (gst_util_greatest_common_divisor (-2, 3) != 1);
  fail_if (gst_util_greatest_common_divisor (-3, 5) != 1);
  fail_if (gst_util_greatest_common_divisor (-1, -1) != 1);
  fail_if (gst_util_greatest_common_divisor (-2, -3) != 1);
  fail_if (gst_util_greatest_common_divisor (-3, -5) != 1);
  fail_if (gst_util_greatest_common_divisor (1, -1) != 1);
  fail_if (gst_util_greatest_common_divisor (2, -3) != 1);
  fail_if (gst_util_greatest_common_divisor (3, -5) != 1);
  fail_if (gst_util_greatest_common_divisor (2, 2) != 2);
  fail_if (gst_util_greatest_common_divisor (2, 4) != 2);
  fail_if (gst_util_greatest_common_divisor (1001, 11) != 11);

}

GST_END_TEST;

GST_START_TEST (test_read_macros)
{
  guint8 carray[] = "ABCDEFGH"; /* 0x41 ... 0x48 */
  guint32 uarray[2];
  guint8 *cpointer;

  memcpy (uarray, carray, 8);
  cpointer = carray;

  /* 16 bit */
  /* First try the standard pointer variants */
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (cpointer), 0x4142);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (cpointer + 1), 0x4243);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (cpointer + 2), 0x4344);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (cpointer + 3), 0x4445);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (cpointer + 4), 0x4546);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (cpointer + 5), 0x4647);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (cpointer + 6), 0x4748);

  fail_unless_equals_int_hex (GST_READ_UINT16_LE (cpointer), 0x4241);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (cpointer + 1), 0x4342);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (cpointer + 2), 0x4443);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (cpointer + 3), 0x4544);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (cpointer + 4), 0x4645);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (cpointer + 5), 0x4746);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (cpointer + 6), 0x4847);

  /* On an array of guint8 */
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (carray), 0x4142);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (carray + 1), 0x4243);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (carray + 2), 0x4344);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (carray + 3), 0x4445);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (carray + 4), 0x4546);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (carray + 5), 0x4647);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (carray + 6), 0x4748);

  fail_unless_equals_int_hex (GST_READ_UINT16_LE (carray), 0x4241);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (carray + 1), 0x4342);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (carray + 2), 0x4443);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (carray + 3), 0x4544);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (carray + 4), 0x4645);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (carray + 5), 0x4746);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (carray + 6), 0x4847);

  /* On an array of guint32 */
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (uarray), 0x4142);
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (uarray + 1), 0x4546);

  fail_unless_equals_int_hex (GST_READ_UINT16_LE (uarray), 0x4241);
  fail_unless_equals_int_hex (GST_READ_UINT16_LE (uarray + 1), 0x4645);


  /* 24bit */
  /* First try the standard pointer variants */
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (cpointer), 0x414243);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (cpointer + 1), 0x424344);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (cpointer + 2), 0x434445);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (cpointer + 3), 0x444546);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (cpointer + 4), 0x454647);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (cpointer + 5), 0x464748);

  fail_unless_equals_int_hex (GST_READ_UINT24_LE (cpointer), 0x434241);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (cpointer + 1), 0x444342);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (cpointer + 2), 0x454443);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (cpointer + 3), 0x464544);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (cpointer + 4), 0x474645);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (cpointer + 5), 0x484746);

  /* On an array of guint8 */
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (carray), 0x414243);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (carray + 1), 0x424344);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (carray + 2), 0x434445);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (carray + 3), 0x444546);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (carray + 4), 0x454647);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (carray + 5), 0x464748);

  fail_unless_equals_int_hex (GST_READ_UINT24_LE (carray), 0x434241);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (carray + 1), 0x444342);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (carray + 2), 0x454443);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (carray + 3), 0x464544);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (carray + 4), 0x474645);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (carray + 5), 0x484746);

  /* On an array of guint32 */
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (uarray), 0x414243);
  fail_unless_equals_int_hex (GST_READ_UINT24_BE (uarray + 1), 0x454647);

  fail_unless_equals_int_hex (GST_READ_UINT24_LE (uarray), 0x434241);
  fail_unless_equals_int_hex (GST_READ_UINT24_LE (uarray + 1), 0x474645);


  /* 32bit */
  /* First try the standard pointer variants */
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (cpointer), 0x41424344);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (cpointer + 1), 0x42434445);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (cpointer + 2), 0x43444546);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (cpointer + 3), 0x44454647);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (cpointer + 4), 0x45464748);

  fail_unless_equals_int_hex (GST_READ_UINT32_LE (cpointer), 0x44434241);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (cpointer + 1), 0x45444342);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (cpointer + 2), 0x46454443);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (cpointer + 3), 0x47464544);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (cpointer + 4), 0x48474645);

  /* On an array of guint8 */
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (carray), 0x41424344);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (carray + 1), 0x42434445);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (carray + 2), 0x43444546);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (carray + 3), 0x44454647);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (carray + 4), 0x45464748);

  fail_unless_equals_int_hex (GST_READ_UINT32_LE (carray), 0x44434241);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (carray + 1), 0x45444342);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (carray + 2), 0x46454443);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (carray + 3), 0x47464544);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (carray + 4), 0x48474645);

  /* On an array of guint32 */
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (uarray), 0x41424344);
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (uarray + 1), 0x45464748);

  fail_unless_equals_int_hex (GST_READ_UINT32_LE (uarray), 0x44434241);
  fail_unless_equals_int_hex (GST_READ_UINT32_LE (uarray + 1), 0x48474645);


  /* 64bit */
  fail_unless_equals_int64_hex (GST_READ_UINT64_BE (cpointer),
      0x4142434445464748);
  fail_unless_equals_int64_hex (GST_READ_UINT64_LE (cpointer),
      0x4847464544434241);

  fail_unless_equals_int64_hex (GST_READ_UINT64_BE (carray),
      0x4142434445464748);
  fail_unless_equals_int64_hex (GST_READ_UINT64_LE (carray),
      0x4847464544434241);

  fail_unless_equals_int64_hex (GST_READ_UINT64_BE (uarray),
      0x4142434445464748);
  fail_unless_equals_int64_hex (GST_READ_UINT64_LE (uarray),
      0x4847464544434241);

  /* make sure the data argument is not duplicated inside the macro
   * with possibly unexpected side-effects */
  cpointer = carray;
  fail_unless_equals_int (GST_READ_UINT8 (cpointer++), 'A');
  fail_unless (cpointer == carray + 1);

  cpointer = carray;
  fail_unless_equals_int_hex (GST_READ_UINT16_BE (cpointer++), 0x4142);
  fail_unless (cpointer == carray + 1);

  cpointer = carray;
  fail_unless_equals_int_hex (GST_READ_UINT32_BE (cpointer++), 0x41424344);
  fail_unless (cpointer == carray + 1);

  cpointer = carray;
  fail_unless_equals_int64_hex (GST_READ_UINT64_BE (cpointer++),
      0x4142434445464748);
  fail_unless (cpointer == carray + 1);
}

GST_END_TEST;

GST_START_TEST (test_write_macros)
{
  guint8 carray[8];
  guint8 *cpointer;

  /* make sure the data argument is not duplicated inside the macro
   * with possibly unexpected side-effects */
  memset (carray, 0, sizeof (carray));
  cpointer = carray;
  GST_WRITE_UINT8 (cpointer++, 'A');
  fail_unless_equals_pointer (cpointer, carray + 1);
  fail_unless_equals_int (carray[0], 'A');

  memset (carray, 0, sizeof (carray));
  cpointer = carray;
  GST_WRITE_UINT16_BE (cpointer++, 0x4142);
  fail_unless_equals_pointer (cpointer, carray + 1);
  fail_unless_equals_int (carray[0], 'A');
  fail_unless_equals_int (carray[1], 'B');

  memset (carray, 0, sizeof (carray));
  cpointer = carray;
  GST_WRITE_UINT32_BE (cpointer++, 0x41424344);
  fail_unless_equals_pointer (cpointer, carray + 1);
  fail_unless_equals_int (carray[0], 'A');
  fail_unless_equals_int (carray[3], 'D');

  memset (carray, 0, sizeof (carray));
  cpointer = carray;
  GST_WRITE_UINT64_BE (cpointer++, 0x4142434445464748);
  fail_unless_equals_pointer (cpointer, carray + 1);
  fail_unless_equals_int (carray[0], 'A');
  fail_unless_equals_int (carray[7], 'H');

  memset (carray, 0, sizeof (carray));
  cpointer = carray;
  GST_WRITE_UINT16_LE (cpointer++, 0x4142);
  fail_unless_equals_pointer (cpointer, carray + 1);
  fail_unless_equals_int (carray[0], 'B');
  fail_unless_equals_int (carray[1], 'A');

  memset (carray, 0, sizeof (carray));
  cpointer = carray;
  GST_WRITE_UINT32_LE (cpointer++, 0x41424344);
  fail_unless_equals_pointer (cpointer, carray + 1);
  fail_unless_equals_int (carray[0], 'D');
  fail_unless_equals_int (carray[3], 'A');

  memset (carray, 0, sizeof (carray));
  cpointer = carray;
  GST_WRITE_UINT64_LE (cpointer++, 0x4142434445464748);
  fail_unless_equals_pointer (cpointer, carray + 1);
  fail_unless_equals_int (carray[0], 'H');
  fail_unless_equals_int (carray[7], 'A');
}

GST_END_TEST;
static Suite *
gst_utils_suite (void)
{
  Suite *s = suite_create ("GstUtils");
  TCase *tc_chain = tcase_create ("general");

  tcase_set_timeout (tc_chain, 800);

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_buffer_probe_n_times);
  tcase_add_test (tc_chain, test_buffer_probe_once);
  tcase_add_test (tc_chain, test_math_scale);
  tcase_add_test (tc_chain, test_math_scale_round);
  tcase_add_test (tc_chain, test_math_scale_ceil);
  tcase_add_test (tc_chain, test_math_scale_uint64);
  tcase_add_test (tc_chain, test_math_scale_random);
#ifdef HAVE_GSL
#ifdef HAVE_GMP
  tcase_add_test (tc_chain, test_math_scale_gmp);
  tcase_add_test (tc_chain, test_math_scale_gmp_int);
#endif
#endif

  tcase_add_test (tc_chain, test_guint64_to_gdouble);
  tcase_add_test (tc_chain, test_gdouble_to_guint64);
#ifndef GST_DISABLE_PARSE
  tcase_add_test (tc_chain, test_parse_bin_from_description);
#endif
  tcase_add_test (tc_chain, test_element_found_tags);
  tcase_add_test (tc_chain, test_element_link);
  tcase_add_test (tc_chain, test_element_unlink);
  tcase_add_test (tc_chain, test_set_value_from_string);
  tcase_add_test (tc_chain, test_binary_search);

  tcase_add_test (tc_chain, test_pad_proxy_query_caps_aggregation);
  tcase_add_test (tc_chain, test_greatest_common_divisor);

  tcase_add_test (tc_chain, test_read_macros);
  tcase_add_test (tc_chain, test_write_macros);
  return s;
}

GST_CHECK_MAIN (gst_utils);
