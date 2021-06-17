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

#include <pcie_src.h>

GST_DEBUG_CATEGORY_EXTERN (pciesrc_sink_debug);
#define GST_CAT_DEFAULT pciesrc_sink_debug

gboolean feed_data (GstElement * pciesrc, guint size, App * app) {
    GstBuffer *buffer;
    guint64 len;
    GstFlowReturn ret;
    app->frame_cnt++;
    if (app->use_case == VGST_DECODE_MP4_TO_YUV || \
        app->use_case == VGST_TRANSCODE_MP4_TO_TS) {
      g_mutex_lock (&app->mutex);
      if (app->read_offset >= app->length) {
        /* we are at EOS, send end-of-stream */
        if (!app->eos_flag) {
          GST_DEBUG ("Emitting eos");
          g_signal_emit_by_name (app->pciesrc, "end-of-stream", &ret);
        }
        g_mutex_unlock (&app->mutex);
        return TRUE;
      }

      /* read any amount of data, we are allowed to return less if we are EOS */
      buffer = gst_buffer_new ();
      len = CHUNK_SIZE;

      if (app->read_offset + len > app->length)
        len = app->length - app->read_offset;

      if (app->total_len < ONE_GB_SIZE) {
        pcie_read(app->fd, len, app->read_offset, app->data + app->read_offset);

        gst_buffer_append_memory (buffer,
          gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,            \
                                  app->data, app->total_len,           \
                                  app->read_offset, len, NULL, NULL)); \
      } else {
        if (app->intOffset + len >  app->total_len)
          app->intOffset = 0;

        pcie_read(app->fd, len, app->read_offset, app->data + app->intOffset);

        gst_buffer_append_memory (buffer,
          gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,          \
                                  app->data, app->total_len,         \
                                  app->intOffset, len, NULL, NULL)); \
        app->intOffset += len;
      }

      GST_DEBUG ("Feed buffer %p, offset %" G_GUINT64_FORMAT "-%lu", \
                  buffer, app->read_offset, len);
      g_signal_emit_by_name (app->pciesrc, "push-buffer", buffer, &ret);
      gst_buffer_unref (buffer);

      app->read_offset += len;
      g_mutex_unlock (&app->mutex);

    } else if (app->use_case == VGST_ENCODE_YUV_TO_MP4) {
      len = app->yuv_frame_size;
      if (app->read_offset >= app->length) {
        if (!app->eos_flag) {
          GST_DEBUG ("Emitting eos");
          g_signal_emit_by_name (app->pciesrc, "end-of-stream", &ret);
        }
        return TRUE;
      }

      buffer = gst_buffer_new ();

      if (app->read_offset + len > app->length) {
          if (!app->eos_flag) {
            GST_DEBUG ("Emitting eos");
            g_signal_emit_by_name (app->pciesrc, "end-of-stream", &ret);
          }
          return TRUE;
      }

      if (app->intOffset + len >  app->total_len)
        app->intOffset = 0;

      pcie_read(app->fd, len, app->read_offset, app->data + app->intOffset);

      gst_buffer_append_memory (buffer,
        gst_memory_new_wrapped (GST_MEMORY_FLAG_READONLY,          \
                                app->data, app->length,            \
                                app->intOffset, len, NULL, NULL)); \

      GST_BUFFER_TIMESTAMP(buffer) = (GstClockTime)
                                     ((app->frame_cnt/(float)app->fps) * 1e9);
      GST_DEBUG ("Frame cnt -%lu", app->frame_cnt);
      GST_DEBUG ("Feed buffer %p, offset %" G_GUINT64_FORMAT "-%lu", buffer, \
                 app->read_offset, len);
      g_signal_emit_by_name (app->pciesrc, "push-buffer", buffer, &ret);

      gst_buffer_unref (buffer);
      app->intOffset += len;
      app->read_offset += len;
    }
    return TRUE;
}

void start_feed (GstElement *source, guint size, App *data) {
    if (data->sourceid == 0) {
      data->sourceid = g_idle_add ((GSourceFunc) feed_data, data);
    }
}

void stop_feed (GstElement *source, App *data) {
    if (data->sourceid != 0) {
      g_source_remove (data->sourceid);
      data->sourceid  = 0;
      data->frame_cnt = 0;
    }
}

gboolean seek_data (GstElement * pciesrc, guint64 position, App * app) {
    g_mutex_lock (&app->mutex);
    GST_DEBUG ("Seek to offset %" G_GUINT64_FORMAT, position);
    app->read_offset = position;
    g_mutex_unlock (&app->mutex);
    return TRUE;
}
