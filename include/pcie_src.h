/*********************************************************************
 * copyright (C) 2017-2021 Xilinx, Inc.
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

#ifndef INCLUDE_PCIE_SRC_H_
#define INCLUDE_PCIE_SRC_H_

#include <pcie_main.h>

/* This method is called by the need-data signal callback, we feed data into the
 * appsrc with an arbitrary size.
 */
gboolean feed_data (GstElement * appsrc, guint size, App * app);

/* This signal callback triggers when appsrc needs data. Here, we add an idle
 * handler to the mainloop to start pushing data into the appsrc
 */
void start_feed (GstElement *source, guint size, App *data);

/* This callback triggers when appsrc has enough data and we can stop sending.
 * We remove the idle handler from the mainloop
 */
void stop_feed (GstElement *source, App *data);

/* called when appsrc wants us to return data from a new position with the next
 * call to push-buffer
 */
gboolean seek_data (GstElement * appsrc, guint64 position, App * app);

#endif /* INCLUDE_PCIE_SRC_H_ */
