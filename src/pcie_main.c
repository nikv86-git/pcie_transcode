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

#include <pcie_main.h>
#include <pcie_src.h>
#include <unistd.h> // For sleep()

#define GET_START           0x1e
#define GET_FPS             0xb

App s_app;

GST_DEBUG_CATEGORY (pciesrc_sink_debug);
#define GST_CAT_DEFAULT pciesrc_sink_debug

gboolean bus_message (GstBus * bus, GstMessage * message, App * app) {
    switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      GST_ERROR ("Received error from %s", GST_MESSAGE_SRC_NAME (message));
      if (app->loop && g_main_is_running (app->loop)) {
        g_main_loop_quit (app->loop);
      }
      break;
    case GST_MESSAGE_EOS:
      GST_DEBUG ("EOS reached %lu",app->write_offset);
      if (app->write_offset) {
        pcie_write (app->fd, app->write_offset, 0, app->buf);
        app->write_offset = 0;
      }

      if (g_main_loop_is_running (app->loop)) {
        GST_DEBUG ("Qutting the loop");
        if (app->loop && g_main_is_running (app->loop)) {
          g_main_loop_quit (app->loop);
        }
      }
      break;
    default:
      break;
    }
    return TRUE;
}

gboolean handle_keyboard (GIOChannel *source, GIOCondition cond, App *data) {
    gchar *str = NULL;
    GstEvent *event;
    if (g_io_channel_read_line (source, &str, NULL, NULL, NULL) != \
        G_IO_STATUS_NORMAL) {
      return TRUE;
    }
    switch (g_ascii_tolower (str[0])) {
      case 'q': {
        GST_DEBUG ("Quitting the playback");
        event = gst_event_new_eos();
        if (event) {
          if (gst_element_send_event (data->pipeline, event)) {
            data->eos_flag = TRUE;
            GST_DEBUG ("Send event to pipeline Succeed");
          } else
            GST_ERROR ("Send event to pipeline failed");
        }
        return FALSE;
      }
      break;
    }
    g_free (str);
    return TRUE;
}

void on_pad_added (GstElement *element,
                   GstPad     *pad,
                   gpointer   data) {
    gchar *str;
    App *play_ptr     = (App *)data;
    GstPad *vidpad    = NULL,       *audpad     = NULL;
    gchar *vidpadname = NULL,       *audpadname = NULL;
    GstCaps *caps     = gst_pad_get_current_caps (pad);

    str = gst_caps_to_string(caps);
    GST_DEBUG ("Caps value is %s", str);

    if (g_str_has_prefix (str, "video/")) {
      if (play_ptr->use_case == VGST_DECODE_MP4_TO_YUV) {
        if (!gst_element_link_many(play_ptr->decodebin, play_ptr->videoqueue, \
                                   NULL))
          GST_ERROR ("Error linking for decodebin --> queue");
        else
          GST_DEBUG ("Linked for decodebin --> queue successfully");

      } else if (play_ptr->use_case == VGST_TRANSCODE_MP4_TO_TS) {
        vidpad = gst_element_get_request_pad(play_ptr->mux, "sink_%d");
        vidpadname = gst_pad_get_name(vidpad);
        GST_DEBUG ("Video pad name %s", vidpadname);

        if (!gst_element_link_many(play_ptr->decodebin, play_ptr->videoqueue,   \
                                   play_ptr->videoenc,  play_ptr->enccapsfilter,\
                                   play_ptr->videoparser,
                                   NULL)) {
          GST_ERROR ("Error linking for decodebin --> queue --> videoenc --> "\
                     "enccapsfilter, videoparser");
          goto CLEAN_UP;
        } else {
          GST_DEBUG ("Linked for decodebin --> queue --> videoenc --> "\
                     "enccapsfilter, videoparser");
        }

        if (!gst_element_link_pads(play_ptr->videoparser, "src", play_ptr->mux,\
                                   vidpadname)) {
          GST_ERROR ("Error linking for videoparser --> mux");
          goto CLEAN_UP;
        } else {
          GST_DEBUG ("Linked for videoparser --> mux successfully");
        }
      }
      goto EXIT;
    } else if (g_str_has_prefix (str, "audio/")) {
      if (play_ptr->use_case == VGST_DECODE_MP4_TO_YUV) {
        if (play_ptr->pipeline && play_ptr->fakesink) {
          gst_bin_add_many (GST_BIN(play_ptr->pipeline), play_ptr->fakesink, \
                            NULL);
          GST_DEBUG ("fakesink is added in the pipeline and syncing to patent "\
                     "state");
          gst_element_sync_state_with_parent (play_ptr->fakesink);
        }

        if (!gst_element_link_many(play_ptr->decodebin, play_ptr->fakesink, \
                                   NULL))
          GST_ERROR ("Error linking for decodebin --> fakesink");
        else
          GST_DEBUG ("Linked for decodebin --> fakesink successfully");

      } else if (play_ptr->use_case == VGST_TRANSCODE_MP4_TO_TS) {
        audpad     = gst_element_get_request_pad(play_ptr->mux, "sink_%d");
        audpadname = gst_pad_get_name(audpad);
        GST_DEBUG ("Audio pad name %s", audpadname);

        if (!gst_element_link_many(play_ptr->decodebin,     \
                                   play_ptr->audioqueue,    \
                                   play_ptr->audioconvert,  \
                                   play_ptr->audioresample, \
                                   play_ptr->audioenc,      \
                                   NULL)) {
          GST_ERROR ("Error linking for decodebin --> queue --> audioconvert "\
                     "--> audioresample --> audioenc");
          goto CLEAN_UP;
        } else {
          GST_DEBUG ("Linked for decodebin --> queue --> audioconvert --> "\
                     "audioresample --> audioenc successfully");
        }

        if (!gst_element_link_pads(play_ptr->audioenc, "src", play_ptr->mux, \
            audpadname)) {
          GST_ERROR ("Error linking for audioenc --> mux");
          goto CLEAN_UP;
        } else {
          GST_DEBUG ("Linked for audioenc --> mux successfully");
        }
      }
      goto EXIT;
    }

CLEAN_UP:
    if (audpad) {
      GST_DEBUG ("Releasing audio pad");
      gst_element_release_request_pad (play_ptr->mux, audpad);
    }
    if (vidpad) {
      GST_DEBUG ("Releasing video pad");
      gst_element_release_request_pad (play_ptr->mux, vidpad);
    }
EXIT :
    if (vidpadname)
      g_free (vidpadname);
    if (audpadname)
      g_free (audpadname);

    if (str)
      g_free (str);
    if (caps)
      gst_caps_unref (caps);
}


GstFlowReturn new_sample (GstElement *sink, App *data) {
    GstSample *sample;
    GstBuffer *buffer;
    GstMapInfo map;

    /* get the sample from pciesink */
    g_signal_emit_by_name (sink, "pull-sample", &sample);
    buffer = gst_sample_get_buffer (sample);
    if (!buffer) {
      GST_DEBUG ("Buffer is null");
      return GST_FLOW_EOS;
    }

    gst_buffer_map (buffer, &map, GST_MAP_READ);
    if (data->write_offset + map.size > WRITE_BUF_SIZE) {
      GST_DEBUG ("Writing data of size: %lu\n", data->write_offset);
      pcie_write (data->fd, data->write_offset, 0, data->buf);
      data->write_offset = 0;
    }

    memcpy (data->buf + data->write_offset, map.data, map.size);

    data->write_offset += map.size;

    gst_buffer_unmap (buffer, &map);

    /* Don't need the sample anymore */
    gst_sample_unref (sample);

    return GST_FLOW_OK;
}

void set_property (App *app) {
    gchar *profile    = NULL;
    GstCaps *enc_caps = NULL;
    guint64 mp_size   = 0;
    /* set encoder profile based on input format */
    if (app->input_format == NV12) {
      profile = app->enc_param.profile == BASELINE_PROFILE ?              \
                "constrained-baseline" :                                  \
                app->enc_param.profile == MAIN_PROFILE ? "main" : "high"; \
    } else if (app->input_format == NV16) {
      profile = app->enc_param.enc_type == HEVC ? "main-422"    : "high-4:2:2";
    } else if (app->input_format == XV15) {
      profile = app->enc_param.enc_type == HEVC ? "main-10"     : "high-10";
    } else if (app->input_format == XV20) {
      profile = app->enc_param.enc_type == HEVC ? "main-422-10" : "high-4:2:2";
    }

    /* Calculate max-picture-size */
    if (app->enc_param.max_picture_size == MAX_PICTURE_SIZE_ENABLE) {
      mp_size = (app->enc_param.bitrate/app->fps) * ALLOWEDPEAKMARGIN;
      g_object_set (G_OBJECT (app->videoenc),                    \
                  "max-picture-size", mp_size,                   \
                  NULL);
    }

    /* set to read from pciesrc */
    if (app->use_case == VGST_DECODE_MP4_TO_YUV || \
        app->use_case == VGST_TRANSCODE_MP4_TO_TS) {
      g_object_set (G_OBJECT (app->pciesrc),                     \
                    "stream-type", GST_APP_STREAM_TYPE_SEEKABLE, \
                    "format",      GST_FORMAT_BYTES,             \
                    "size",        app->length,                  \
                    NULL);
      g_object_set (G_OBJECT (app->pciesrc),                     \
                    "max-bytes", CHUNK_SIZE,                     \
                    NULL);
    } else if (app->use_case == VGST_ENCODE_YUV_TO_MP4) {
      GST_DEBUG ("setting pciesrc plugin");
      g_object_set (G_OBJECT (app->pciesrc),                   \
                    "stream-type", GST_APP_STREAM_TYPE_STREAM, \
                    "format",      GST_FORMAT_TIME,            \
                    "is-live",     TRUE,                       \
                    NULL);
      g_object_set (G_OBJECT (app->pciesrc),                   \
                    "max-bytes", app->yuv_frame_size * 10,     \
                    NULL);
    }

    g_object_set (app->audioqueue, "max-size-bytes", 0, NULL);
    g_object_set (app->videoqueue, "max-size-bytes", 0, NULL);

    /* Configure encoder */
    g_object_set (G_OBJECT (app->videoenc),             \
                  "gop-length", app->enc_param.gop_len, \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),             \
                  "gop-mode", app->enc_param.gop_mode,  \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),                      \
                  "low-bandwidth", app->enc_param.low_bandwidth, \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),                 \
                  "target-bitrate", app->enc_param.bitrate, \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),           \
                  "b-frames", app->enc_param.b_frame, \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),           \
                  "num-slices", app->enc_param.slice, \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),               \
                  "control-rate", app->enc_param.rc_mode, \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),          \
                  "qp-mode", app->enc_param.qp_mode, \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),                          \
                  "prefetch-buffer", app->enc_param.enable_l2Cache,  \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),                          \
                  "min-qp", app->enc_param.min_qp,                   \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),                          \
                  "max-qp", app->enc_param.max_qp,                   \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),                          \
                  "cpb-size", app->enc_param.cpb_size,               \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),                          \
                  "initial-delay", app->enc_param.initial_delay,     \
                  NULL);
    g_object_set (G_OBJECT (app->videoenc),                          \
                  "periodicity-idr", app->enc_param.periodicity_idr, \
                  NULL);
    if (app->enc_param.rc_mode == CBR)
      g_object_set (G_OBJECT (app->videoenc),                  \
                    "filler-data", app->enc_param.filler_data, \
                    NULL);
    GST_DEBUG ("gop-len %d",        app->enc_param.gop_len);
    GST_DEBUG ("gop_mode %d",       app->enc_param.gop_mode);
    GST_DEBUG ("low_bandwidth %d",  app->enc_param.low_bandwidth);
    GST_DEBUG ("bitrate %d",        app->enc_param.bitrate);
    GST_DEBUG ("b_frame %d",        app->enc_param.b_frame);
    GST_DEBUG ("slice %d",          app->enc_param.slice);
    GST_DEBUG ("rc_mode %d",        app->enc_param.rc_mode);
    GST_DEBUG ("qp_mode %d",        app->enc_param.qp_mode);
    GST_DEBUG ("enable_l2Cache %d", app->enc_param.enable_l2Cache);
    GST_DEBUG ("filler_data %d",    app->enc_param.filler_data);
    GST_DEBUG ("min_qp %d",         app->enc_param.min_qp);
    GST_DEBUG ("max_qp %d",         app->enc_param.max_qp);
    GST_DEBUG ("cpb_size %d",       app->enc_param.cpb_size);
    GST_DEBUG ("initial_delay %d",  app->enc_param.initial_delay);
    GST_DEBUG ("periodicity_idr %d",app->enc_param.periodicity_idr);

    /* Configure pciesink */
    g_object_set (app->pciesink, "emit-signals", TRUE,  NULL);
    g_object_set (app->pciesink, "sync",         FALSE, NULL);
    g_object_set (app->pciesink, "async",        FALSE, NULL);

    /* Configure video parse */
    g_object_set (app->vparse, "width",     app->input_res.width,  NULL);
    g_object_set (app->vparse, "height",    app->input_res.height, NULL);
    g_object_set (app->vparse, "framerate", app->fps, 1, NULL);
    if (app->input_format == NV12)
      g_object_set (app->vparse, "format", VIDEOPARSE_FORMAT_NV12, NULL);
    else if (app->input_format == NV16)
      g_object_set (app->vparse, "format", VIDEOPARSE_FORMAT_NV16, NULL);
    else if (app->input_format == XV15)
      g_object_set (app->vparse, "format", VIDEOPARSE_FORMAT_XV15, NULL);
    else if (app->input_format == XV20)
      g_object_set (app->vparse, "format", VIDEOPARSE_FORMAT_XV20, NULL);

    /* set encoder caps */
    if (app->enc_param.enc_type == AVC)
      enc_caps = gst_caps_new_simple ("video/x-h264",                      \
                                      "profile",   G_TYPE_STRING, profile, \
                                      "alignment", G_TYPE_STRING, "au",    \
                                      NULL);
    else if (app->enc_param.enc_type == HEVC)
      enc_caps = gst_caps_new_simple ("video/x-h265",                      \
                                      "profile",   G_TYPE_STRING, profile, \
                                      "alignment", G_TYPE_STRING, "au",    \
                                      NULL);
    g_object_set (G_OBJECT (app->enccapsfilter),  "caps",  enc_caps, NULL);
    gst_caps_unref (enc_caps);
}

gint link_yuv_pipeline (App *app) {
    gint ret = 0;
    GstPad *vidpad    = NULL;
    gchar *vidpadname = NULL;
    vidpad     = gst_element_get_request_pad(app->mux, "sink_%d");
    vidpadname = gst_pad_get_name(vidpad);
    GST_DEBUG ("Video pad name %s", vidpadname);

    if (!gst_element_link_many (app->pciesrc,       app->vparse,  app->videoenc,\
                                app->enccapsfilter, app->videoparser,           \
                                NULL)) {
      GST_ERROR ("Error linking for pciesrc --> vparse --> videoenc --> "\
                 "enccapsfilter --> videoparser");
      ret = PCIE_TRANSCODE_APP_FAIL;
    } else {
      GST_DEBUG ("Linked for for pciesrc --> vparse --> videoenc --> "\
                 "enccapsfilter --> videoparser");
    }

    if (!gst_element_link_pads(app->videoparser, "src", app->mux, vidpadname)) {
      GST_ERROR ("Error linking for videoparser --> mux");
      ret = PCIE_TRANSCODE_APP_FAIL;
    } else {
      GST_DEBUG ("Linked for videoparser --> mux successfully");
    }

    if (vidpadname)
      g_free (vidpadname);
    return ret;
}

gint create_pipeline (App *app) {
    gint ret = 0;

    if (app->use_case == VGST_ENCODE_YUV_TO_MP4) {
      gst_bin_add_many (GST_BIN (app->pipeline),                        \
                        app->pciesrc,  app->videoqueue,    app->vparse, \ 
                        app->pciesink, app->videoparser,   app->mux,    \
                        app->videoenc, app->enccapsfilter,              \
                        NULL);

      ret = link_yuv_pipeline (app);
      if (ret < 0 )
        return ret;
    } else if (app->use_case == VGST_DECODE_MP4_TO_YUV) {
      gst_bin_add_many (GST_BIN (app->pipeline), app->pciesrc, app->decodebin, \
                        app->audioqueue, app->videoqueue, app->pciesink,       \
                        NULL);
      g_signal_connect (app->pciesrc,   "seek-data", G_CALLBACK (seek_data),   \
                        app);
      g_signal_connect (app->decodebin, "pad-added", G_CALLBACK (on_pad_added),\
                        app);
    } else if (app->use_case == VGST_TRANSCODE_MP4_TO_TS) {
      gst_bin_add_many (GST_BIN (app->pipeline),                               \
                        app->pciesrc,  app->decodebin,     app->audioqueue,    \
                        app->pciesink, app->videoparser,   app->audioconvert,  \
                        app->videoenc, app->videoqueue,    app->enccapsfilter, \
                        app->audioenc, app->audioresample, app->tee,           \
                        app->mux,                                              \
                        NULL);
      g_signal_connect (app->pciesrc,   "seek-data", G_CALLBACK (seek_data),   \
                        app);
      g_signal_connect (app->decodebin, "pad-added", G_CALLBACK (on_pad_added),\
                        app);
    }

    if (app->use_case == VGST_DECODE_MP4_TO_YUV || \
        app->use_case == VGST_TRANSCODE_MP4_TO_TS) {
      if (gst_element_link_many (app->pciesrc, app->decodebin, NULL) != TRUE) {
        GST_ERROR ("Error linking for pciesrc --> decodebin");
        ret = PCIE_TRANSCODE_APP_FAIL;
      }
    }

    if (app->use_case == VGST_ENCODE_YUV_TO_MP4 || \
        app->use_case == VGST_TRANSCODE_MP4_TO_TS) {
      if (!gst_element_link_many (app->mux, app->pciesink, NULL)) {
        GST_ERROR ("Error linking for mux --> pciesink");
        ret = PCIE_TRANSCODE_APP_FAIL;
      } else {
        GST_DEBUG ("Linked for mux --> pciesink successfully");
      }
    } else if (app->use_case == VGST_DECODE_MP4_TO_YUV) {
      if (!gst_element_link_many (app->videoqueue, app->pciesink, NULL)) {
        GST_ERROR ("Error linking for queue --> pciesink");
        ret = PCIE_TRANSCODE_APP_FAIL;
      } else {
        GST_DEBUG ("Linked for queue--> pciesink successfully");
      }
    }

    return ret;
}

gint main (gint argc, gchar *argv[]) {
    App *app = &s_app;
    GstBus *bus;
    GIOChannel *io_stdin;
    ssize_t rc;
    gint ret = 0;
    app->use_case = VGST_TRANSCODE_MP4_TO_TS;

    gst_init (&argc, &argv);
    memset (app, 0, sizeof(App));
    GST_DEBUG_CATEGORY_INIT (pciesrc_sink_debug, "pcie-transcode", 0,
                             "pcie-transcode pipeline example");

    app->frame_cnt = 0;
    app->sourceid  = 0;

    int ret_bit = 0;
    int start_bit = 0;
	unsigned int fps;
    
        
    /* try to open the file as an mmapped file */
    app->fd = pcie_open();
    if (app->fd < 0) {
      GST_ERROR ("Unable to open device %d", app->fd);
      goto FAIL;
    }
    /* get some vitals, this will be used to read data from the mmapped file
	 * and feed it to pciesrc. */
    app->length  = pcie_get_file_length(app->fd);
    if (app->length <= 0) {
      GST_ERROR ("Unable to get file_length");
      goto FAIL;
    }

	
    if (app->fd > 0) {
        ret_bit = ioctl(app->fd, GET_FPS, &fps);
		app->fs = fps;
        if (ret_bit < 0)
            printf("unable to run ioctl for FPS.\n");
        else
            printf("Input FPS is %u\n", fps);
    }
	
	printf("start_bit is %d.\n", start_bit);
    /* try to wait for start from host*/
    while(!start_bit) {
      if (app->fd > 0) {
          ret_bit = ioctl(app->fd, GET_START, &start_bit);
          if (ret_bit < 0)
              printf("unable to run ioctl for start_bit.\n");
          else
              printf("start_bit is %d.\n", start_bit);
		  
		  sleep(5);
      }
    }
    /* get some vitals, this will be used to read data from the mmapped file
	 * and feed it to pciesrc. */
    app->length  = pcie_get_file_length(app->fd);
    if (app->length <= 0) {
      GST_ERROR ("Unable to get file_length");
      goto FAIL;
    }

    ret = pcie_get_input_resolution (app->fd, &app->input_res);
    if (ret < 0) {
      GST_ERROR ("Unable to get input resolution");
      goto FAIL;
    }

    app->use_case = pcie_get_use_case_type (app->fd);
    if (app->use_case < 0) {
      GST_ERROR ("Unable to get input use case type");
      goto FAIL;
    }

    ret = pcie_get_enc_params (app->fd, &app->enc_param);
    if (ret < 0) {
      GST_ERROR ("Unable to get encoder params");
      goto FAIL;
    }

    app->input_format = pcie_get_format(app->fd);
    if (app->input_format < 0){
      GST_ERROR ("Unable to get format type");
      goto FAIL;
    } else if (app->input_format != NV12 && app->input_format != NV16 && \
               app->input_format != XV15 && app->input_format != XV20) {
      GST_ERROR ("Provided format is not supported");
      goto FAIL;
    }

    if (app->use_case == VGST_ENCODE_YUV_TO_MP4) {
      app->fps = pcie_get_fps (app->fd);
      if (app->fps < 0) {
        GST_ERROR ("Unable to get input FPS");
        goto FAIL;
      } else if (app->fps > MAX_FPS) {
        GST_ERROR ("%u FPS is not supported", app->fps);
        goto FAIL;
      }
    }

    if (app->input_format == NV12)
      app->yuv_frame_size = app->input_res.width * app->input_res.height * \
                            NV12_MULTIPLIER;
    else if (app->input_format == NV16)
      app->yuv_frame_size = app->input_res.width * app->input_res.height * \
                            NV16_MULTIPLIER;
    else if (app->input_format == XV15)
      app->yuv_frame_size = app->input_res.width * app->input_res.height * \
                            XV15_MULTIPLIER;
    else if (app->input_format == XV20)
      app->yuv_frame_size = app->input_res.width * app->input_res.height * \
                            XV20_MULTIPLIER;

    app->write_offset   = 0;
    app->read_offset    = 0;
    app->intOffset      = 0;
    app->eos_flag       = FALSE;

    /* create a mainloop to get messages */
    app->loop = g_main_loop_new (NULL, TRUE);

    app->pipeline      = gst_pipeline_new ("pipeline");
    app->pciesrc       = gst_element_factory_make ("appsrc",        NULL);
    app->decodebin     = gst_element_factory_make ("decodebin",     NULL);
    app->audioqueue    = gst_element_factory_make ("queue",         "aqueue");
    app->videoqueue    = gst_element_factory_make ("queue",         "vqueue");
    app->vparse        = gst_element_factory_make ("videoparse",    NULL);
    app->audioenc      = gst_element_factory_make ("faac",          NULL);
    app->mux           = gst_element_factory_make ("mpegtsmux",     NULL);
    app->audioconvert  = gst_element_factory_make ("audioconvert",  NULL);
    app->audioresample = gst_element_factory_make ("audioresample", NULL);
    app->pciesink      = gst_element_factory_make ("appsink",       NULL);
    app->tee           = gst_element_factory_make ("tee",           NULL);
    app->displaysink   = gst_element_factory_make ("kmssink",       NULL);
    app->fakesink      = gst_element_factory_make ("fakesink",      NULL);
    app->enccapsfilter = gst_element_factory_make ("capsfilter",    NULL);

    if (app->enc_param.enc_type == AVC) {
      app->videoenc    = gst_element_factory_make ("omxh264enc",    NULL);
      app->videoparser = gst_element_factory_make ("h264parse",     NULL);
    } else if (app->enc_param.enc_type == HEVC){
      app->videoenc    = gst_element_factory_make ("omxh265enc",    NULL);
      app->videoparser = gst_element_factory_make ("h265parse",     NULL);
    } else {
      GST_ERROR ("Unable to support provided encoder type");
    }

    if (!app->pipeline || !app->audioqueue  || !app->audioresample || \
        !app->pciesrc  || !app->videoqueue  || !app->enccapsfilter || \ 
        !app->pciesink || !app->decodebin   || !app->audioconvert  || \
        !app->videoenc || !app->videoparser || !app->audioenc      || \
        !app->fakesink || !app->displaysink || !app->vparse        || \
        !app->tee      || !app->mux) {
      GST_ERROR ("Unable to create required elements");
      goto FAIL;
    }
    io_stdin = g_io_channel_unix_new (fileno (stdin));
    g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc)handle_keyboard, app);

    bus = gst_pipeline_get_bus (GST_PIPELINE (app->pipeline));

    /* add watch for messages */
    gst_bus_add_watch (bus, (GstBusFunc) bus_message, app);
    GST_DEBUG ("Change the settings");

    set_property (app);

    g_signal_connect (app->pciesrc,  "need-data",   G_CALLBACK (start_feed), \
                      app);
    g_signal_connect (app->pciesrc,  "enough-data", G_CALLBACK (stop_feed),  \
                      app);
    g_signal_connect (app->pciesink, "new-sample",  G_CALLBACK (new_sample), \
                      app);

    /* create GStreamer pipeline */
    ret = create_pipeline(app);
    if (ret < 0) {
      GST_ERROR ("Unable to create pipeline");
      goto FAIL;
    }

    if (app->length < ONE_GB_SIZE) {
      app->total_len = app->length;
    } else {
      app->total_len = ONE_GB_SIZE;
    }

    GST_DEBUG ("Setting size %lu", app->total_len);
    app->data = g_malloc0(app->total_len);
    /* go to playing and wait in a mainloop. */
    gst_element_set_state (app->pipeline, GST_STATE_PLAYING);

    /* this mainloop is stopped when we receive an error or EOS */
    g_main_loop_run (app->loop);

    GST_DEBUG ("Stopping");

    gst_element_set_state (app->pipeline, GST_STATE_NULL);
    GST_DEBUG ("Set transfer done");
    rc = pcie_set_read_transfer_done(app->fd, 0xef);

    if (rc < 0)
      GST_ERROR ("Read transfer done ioctl failed");

    rc = pcie_set_write_transfer_done(app->fd, 0xef);
    if (rc < 0)
      GST_ERROR ("Write transfer done ioctl failed");

FAIL:
    GST_DEBUG ("Close fd");
    if (app->fd >= 0)
      close(app->fd);

    if (app->data) {
      g_free (app->data);
      app->data = NULL;
    }
    if (app->pipeline)
      gst_object_unref (app->pipeline);

    if (bus)
      gst_object_unref (bus);

    if (app->loop && g_main_is_running (app->loop)) {
      g_main_loop_unref (app->loop);
    }
    return 0;
}
