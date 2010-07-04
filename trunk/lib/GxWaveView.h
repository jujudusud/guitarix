/*
 * Copyright (C) 2009, 2010 Hermann Meyer, James Warden, Andreas Degert
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// ***** GtkWaveView.h *****
/******************************************************************************
part of guitarix, plot a wave with Gtk
******************************************************************************/
#ifndef __GX_WAVE_VIEW_H__
#define __GX_WAVE_VIEW_H__

#include <gtk/gtkdrawingarea.h>

G_BEGIN_DECLS

#define GX_TYPE_WAVE_VIEW          (gx_wave_view_get_type())
#define GX_WAVE_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_CAST ((obj), GX_TYPE_WAVE_VIEW, GxWaveView))
#define GX_IS_WAVE_VIEW(obj)       (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GX_TYPE_WAVE_VIEW))
#define GX_WAVE_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_CAST ((klass),  GX_TYPE_WAVE_VIEW, GxWaveViewClass))
#define GX_IS_WAVE_VIEW_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GX_TYPE_WAVE_VIEW))

typedef struct
{
	GtkDrawingArea parent;
	GdkPixbuf *liveview_image;
	const float *frame;
	int frame_size;
	gchar *text_nw;
	gchar *text_ne;
	gchar *text_sw;
	gchar *text_se;
	double text_pos_w;
	double text_pos_e;
} GxWaveView;

typedef struct
{
	GtkDrawingAreaClass parent_class;
} GxWaveViewClass;


GType gx_wave_view_get_type ();

GtkWidget* gx_wave_view_new();
void gx_wave_view_set_frame(GxWaveView *waveview, const float *frame, int frame_size);
void gx_wave_view_set_text(GxWaveView *waveview, const gchar *text, GtkAnchorType pos);

G_END_DECLS

#endif /* __GX_WAVE_VIEW_H__ */