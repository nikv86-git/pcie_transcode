/*********************************************************************
 * Copyright (C) 2017-2021 Xilinx, Inc.
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
 *
 ********************************************************************/

#ifndef INCLUDE_PCIE_MAIN_H_
#define INCLUDE_PCIE_MAIN_H_

#include <stdio.h>
#include <gst/app/gstappsrc.h>
#include <gst/gst.h>
#include <string.h>
#include <pcie_abstract.h>

#define CHUNK_SIZE               (32 * 1024 * 1024)
#define WRITE_BUF_SIZE           (32 * 1024 * 1024)
#define ONE_GB_SIZE              (1024 * 1024 * 1024)
#define CAPS_SIZE                (20)
#define MAX_FPS                  (60)
#define MAX_PICTURE_SIZE_ENABLE  (1)
#define MAX_PICTURE_SIZE_DISABLE (0)
#define ALLOWEDPEAKMARGIN        (1.1)

/* 4k resolution, size = 3840 x 2160 x 1.5 -> bytes */
#define NV12_MULTIPLIER (1.5)
/* 4k resolution, size = 3840 x 2160 x 2 -> bytes */
#define NV16_MULTIPLIER (2.0)
/* 4k resolution, size = 4096 x 2160 x 1.5 x 10/8 = 3840 x 2160 x 2 -> bytes */
#define XV15_MULTIPLIER (2.0)
/* 4k resolution, size = 4096 x 2160 x 2 x 10/8 = 3840 x 2160 x 2.6666667 ->
   bytes */
#define XV20_MULTIPLIER (2.6666667)

#define VIDEOPARSE_FORMAT_NV12	(23)
#define VIDEOPARSE_FORMAT_NV16	(51)
#define VIDEOPARSE_FORMAT_XV15	(79)
#define VIDEOPARSE_FORMAT_XV20	(80)

#define PCIE_TRANSCODE_APP_FAIL (-1)


typedef struct _App App;

struct _App {
    GstElement *audioconvert, *audioresample, *videoparser, *tee;
    GstElement *audioqueue, *audioenc, *mux, *videoenc, *videoqueue;
    GstElement *pipeline, *pciesrc, *pciesink, *displaysink, *vparse;
    GstElement *decodebin, *fakesink, *enccapsfilter;
    GMainLoop *loop;
    GMutex mutex;
    GstPad *vpad, *vpad2;
    guint use_case,     sourceid;         /* To control the GSource */
    guint input_format, fps;
    gint  fd;
    guint64 read_offset, write_offset;
    guint64 length,      total_len, yuv_frame_size;
    guint64 intOffset,   frame_cnt;
    gboolean eos_flag;
    enc_params enc_param;
    resolution input_res;
    gchar *data, *path;
    gchar buf[WRITE_BUF_SIZE];
};

typedef enum {
    VGST_ENCODE_YUV_TO_MP4,
    VGST_DECODE_MP4_TO_YUV,
    VGST_TRANSCODE_MP4_TO_TS,
} PCIE_APP_USE_CASE;

typedef enum {
    CONST_QP,
    VBR,
    CBR,
} VGST_RC_MODE;

typedef enum {
    AVC,
    HEVC,
} VGST_CODEC_TYPE;

typedef enum {
    NV12,
    NV16,
    XV15,
    XV20,
} VGST_FORMAT_TYPE;

typedef enum {
    BASELINE_PROFILE,
    HIGH_PROFILE,
    MAIN_PROFILE,
} VGST_PROFILE_MODE;

/* This API is required for linking src pad of decoder to ts mux element */
void on_pad_added (GstElement *element, GstPad *pad, gpointer   data);

/* This API is required for user interactions to quit the app */
gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, App *data);

/* This API is to capture messages from pipeline */
gboolean bus_message (GstBus * bus, GstMessage * message, App * app);

/* The appsink has received a buffer */
GstFlowReturn new_sample (GstElement *sink, App *data);

#endif /* INCLUDE_PCIE_MAIN_H_ */
