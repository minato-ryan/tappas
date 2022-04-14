/**
* Copyright (c) 2021-2022 Hailo Technologies Ltd. All rights reserved.
* Distributed under the LGPL license (https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt)
**/
#include <gst/gst.h>
#include <gst/video/video.h>
#include <opencv2/opencv.hpp>
#include "overlay/gsthailooverlay.hpp"
#include "common/image.hpp"
#include "overlay/overlay.hpp"
#include "metadata/gst_hailo_meta.hpp"

GST_DEBUG_CATEGORY_STATIC(gst_hailooverlay_debug_category);
#define GST_CAT_DEFAULT gst_hailooverlay_debug_category

/* prototypes */

static void gst_hailooverlay_set_property(GObject *object,
                                          guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_hailooverlay_get_property(GObject *object,
                                          guint property_id, GValue *value, GParamSpec *pspec);
static void gst_hailooverlay_dispose(GObject *object);
static void gst_hailooverlay_finalize(GObject *object);

static gboolean gst_hailooverlay_start(GstBaseTransform *trans);
static gboolean gst_hailooverlay_stop(GstBaseTransform *trans);
static GstFlowReturn gst_hailooverlay_transform_ip(GstBaseTransform *trans,
                                                   GstBuffer *buffer);

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstHailoOverlay, gst_hailooverlay, GST_TYPE_BASE_TRANSFORM,
                        GST_DEBUG_CATEGORY_INIT(gst_hailooverlay_debug_category, "hailooverlay", 0,
                                                "debug category for hailooverlay element"));

enum
{
    PROP_0,
    PROP_LINE_THICKNESS,
    PROP_FONT_THICKNESS,
};

static void
gst_hailooverlay_class_init(GstHailoOverlayClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseTransformClass *base_transform_class =
        GST_BASE_TRANSFORM_CLASS(klass);

    const char *description = "Draws post-processing results for networks inferred by hailonet elements."
                              "\n\t\t\t   "
                              "Draws classes contained by HailoROI objects attached to incoming frames.";
    /* Setting up pads and setting metadata should be moved to
       base_class_init if you intend to subclass this class. */
    gst_element_class_add_pad_template(GST_ELEMENT_CLASS(klass),
                                       gst_pad_template_new("src", GST_PAD_SRC, GST_PAD_ALWAYS,
                                                            gst_caps_from_string(GST_VIDEO_CAPS_MAKE("{ RGB, YUY2 }"))));
    gst_element_class_add_pad_template(GST_ELEMENT_CLASS(klass),
                                       gst_pad_template_new("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
                                                            gst_caps_from_string(GST_VIDEO_CAPS_MAKE("{ RGB, YUY2 }"))));

    gst_element_class_set_static_metadata(GST_ELEMENT_CLASS(klass),
                                          "hailooverlay - overlay element",
                                          "Hailo/Tools",
                                          description,
                                          "hailo.ai <contact@hailo.ai>");

    gobject_class->set_property = gst_hailooverlay_set_property;
    gobject_class->get_property = gst_hailooverlay_get_property;
    g_object_class_install_property(gobject_class, PROP_LINE_THICKNESS,
                                    g_param_spec_int("line-thickness", "line-thickness", "The thickness when drawing lines. Default 1.", 0, G_MAXINT, 1,
                                                     (GParamFlags)(GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    g_object_class_install_property(gobject_class, PROP_FONT_THICKNESS,
                                    g_param_spec_int("font-thickness", "font-thickness", "The thickness when drawing text. Default 1.", 0, G_MAXINT, 1,
                                                     (GParamFlags)(GST_PARAM_MUTABLE_READY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    gobject_class->dispose = gst_hailooverlay_dispose;
    gobject_class->finalize = gst_hailooverlay_finalize;
    base_transform_class->start = GST_DEBUG_FUNCPTR(gst_hailooverlay_start);
    base_transform_class->stop = GST_DEBUG_FUNCPTR(gst_hailooverlay_stop);
    base_transform_class->transform_ip =
        GST_DEBUG_FUNCPTR(gst_hailooverlay_transform_ip);
}

static void
gst_hailooverlay_init(GstHailoOverlay *hailooverlay)
{
    hailooverlay->line_thickness = 1;
    hailooverlay->font_thickness = 1;
}

void gst_hailooverlay_set_property(GObject *object, guint property_id,
                                   const GValue *value, GParamSpec *pspec)
{
    GstHailoOverlay *hailooverlay = GST_HAILO_OVERLAY(object);

    GST_DEBUG_OBJECT(hailooverlay, "set_property");

    switch (property_id)
    {
    case PROP_LINE_THICKNESS:
        hailooverlay->line_thickness = g_value_get_int(value);
        break;
    case PROP_FONT_THICKNESS:
        hailooverlay->font_thickness = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_hailooverlay_get_property(GObject *object, guint property_id,
                                   GValue *value, GParamSpec *pspec)
{
    GstHailoOverlay *hailooverlay = GST_HAILO_OVERLAY(object);

    GST_DEBUG_OBJECT(hailooverlay, "get_property");

    switch (property_id)
    {
    case PROP_LINE_THICKNESS:
        g_value_set_int(value, hailooverlay->line_thickness);
        break;
    case PROP_FONT_THICKNESS:
        g_value_set_int(value, hailooverlay->font_thickness);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_hailooverlay_dispose(GObject *object)
{
    GstHailoOverlay *hailooverlay = GST_HAILO_OVERLAY(object);
    GST_DEBUG_OBJECT(hailooverlay, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_hailooverlay_parent_class)->dispose(object);
}

void gst_hailooverlay_finalize(GObject *object)
{
    GstHailoOverlay *hailooverlay = GST_HAILO_OVERLAY(object);
    GST_DEBUG_OBJECT(hailooverlay, "finalize");

    /* clean up object here */

    G_OBJECT_CLASS(gst_hailooverlay_parent_class)->finalize(object);
}

static gboolean
gst_hailooverlay_start(GstBaseTransform *trans)
{
    GstHailoOverlay *hailooverlay = GST_HAILO_OVERLAY(trans);
    GST_DEBUG_OBJECT(hailooverlay, "start");

    return TRUE;
}

static gboolean
gst_hailooverlay_stop(GstBaseTransform *trans)
{
    GstHailoOverlay *hailooverlay = GST_HAILO_OVERLAY(trans);
    GST_DEBUG_OBJECT(hailooverlay, "stop");

    return TRUE;
}

static GstFlowReturn
gst_hailooverlay_transform_ip(GstBaseTransform *trans,
                              GstBuffer *buffer)
{
    overlay_status_t ret = OVERLAY_STATUS_UNINITIALIZED;
    GstFlowReturn status = GST_FLOW_ERROR;
    GstHailoOverlay *hailooverlay = GST_HAILO_OVERLAY(trans);
    GstCaps *caps;
    cv::Mat mat;
    HailoROIPtr hailo_roi;

    GST_DEBUG_OBJECT(hailooverlay, "transform_ip");

    caps = gst_pad_get_current_caps(trans->sinkpad);
    mat = get_image(buffer, caps, GST_MAP_READWRITE);
    hailo_roi = get_hailo_main_roi(buffer, true);

    // Draw all results of the given roi on mat.
    ret = draw_all(mat, hailo_roi, hailooverlay->font_thickness, hailooverlay->line_thickness);
    if (ret != OVERLAY_STATUS_OK)
    {
        status = GST_FLOW_ERROR;
        goto cleanup;
    }
    status = GST_FLOW_OK;
cleanup:
    mat.release();
    gst_caps_unref(caps);
    return status;
}
