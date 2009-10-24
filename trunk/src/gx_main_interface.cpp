/*
 * Copyright (C) 2009 Hermann Meyer and James Warden
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
 * ---------------------------------------------------------------------------
 *
 *    This is the guitarix GUI main class
 *
 * ----------------------------------------------------------------------------
 */
#include <errno.h>

#include <assert.h>
#include <cstring>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <cstdio>

using namespace std;

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <jack/jack.h>
#include <sndfile.h>

#include "guitarix.h"

using namespace gx_system;
using namespace gx_child_process;
using namespace gx_preset;

namespace gx_gui
{
  // -------------------------------------------------------------
  // GxMainInterface method definitions
  //
  // static member
  bool GxMainInterface::fInitialized = false;

  GxMainInterface::GxMainInterface(const char * name, int* pargc, char*** pargv)
  {
    gtk_init(pargc, pargv);

    /*-- set rc file overwrite it with export--*/
    gtk_rc_parse(rcpath.c_str());

    /*-- Declare the GTK Widgets --*/
    fWindow = gtk_window_new (GTK_WINDOW_TOPLEVEL);

    /*---------------- set window defaults ----------------*/
    gtk_window_set_resizable(GTK_WINDOW (fWindow) , FALSE);
    gtk_window_set_title (GTK_WINDOW (fWindow), name);
    gtk_window_set_gravity(GTK_WINDOW(fWindow), GDK_GRAVITY_NORTH_EAST);

    /*---------------- singnals ----------------*/
    g_signal_connect (GTK_OBJECT (fWindow), "destroy",
		      G_CALLBACK (gx_clean_exit), NULL);

    /*---------------- status icon ----------------*/
    if (gx_pixmap_check() == 0)
      {
	status_icon =    gtk_status_icon_new_from_pixbuf (GDK_PIXBUF(ib));
	gtk_window_set_icon(GTK_WINDOW (fWindow), GDK_PIXBUF(ib));
	g_signal_connect (G_OBJECT (status_icon), "activate", G_CALLBACK (gx_hide_extended_settings), NULL);
	g_signal_connect (G_OBJECT (status_icon), "popup-menu", G_CALLBACK (gx_systray_menu), NULL);
      }
    else
      {
	gx_print_error("Main Interface Constructor",
		       "pixmap check failed, giving up");
	gx_clean_exit(NULL, (gpointer)1);
      }

    /*-- create accelerator group for keyboard shortcuts --*/
    fAccelGroup = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(fWindow), fAccelGroup);

    /*---------------- create boxes ----------------*/
    fTop = 0;
    fBox[fTop] = gtk_vbox_new (homogene, 4);
    fMode[fTop] = kBoxMode;

    /*---------------- add mainbox to main window ---------------*/
    gtk_container_add (GTK_CONTAINER (fWindow), fBox[fTop]);

    fStopped = false;
  }

  //------- create or retrieve unique instance
  GxMainInterface* GxMainInterface::instance(const char* name, int* pargc, char*** pargv)
  {
    static GxMainInterface maingui(name, pargc, pargv);
    return &maingui;
  }


  //------- retrieve jack latency menu item
  GtkWidget* const
  GxMainInterface::getJackLatencyItem(const jack_nframes_t bufsize) const
  {
    int index = (int)(log((float)bufsize)/log(2)) - 5;

    if (index >= 0 && index < NJACKLAT)
      return fJackLatencyItem[index];

    return NULL;
  }

  //------- box stacking up
  void GxMainInterface::pushBox(int mode, GtkWidget* w)
  {
    assert(++fTop < stackSize);
    fMode[fTop] 	= mode;
    fBox[fTop] 		= w;
  }

  void GxMainInterface::closeBox()
  {
    assert(--fTop >= 0);
  }

  //-------- different box styles
  void GxMainInterface::openFrameBox(const char* label)
  {
    GtkWidget * box = gtk_hbox_new (homogene, 2);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);

    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	GtkWidget * frame = addWidget(label, gtk_frame_new (label));
	gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER(frame), box);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }


    /* GtkWidget * box = gtk_frame_new (label);
       gtk_frame_set_shadow_type(GTK_FRAME(box),GTK_SHADOW_ETCHED_OUT);
       pushBox(kSingleMode, addWidget(label, box));*/
  }

  void GxMainInterface::openTabBox(const char* label)
  {
    pushBox(kTabMode, addWidget(label, gtk_notebook_new ()));
  }

  void GxMainInterface::openTextLoggingBox(const char* label)
  {
    GtkWidget* box = gtk_hbox_new (homogene, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 0);

    GtkWidget * scrollbox = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrollbox),
				    GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);

    GtkWidget* frame = addWidget(label, gtk_expander_new(label));
    gtk_container_add (GTK_CONTAINER(frame), box);
    gtk_widget_show(frame);
    gtk_expander_set_expanded(GTK_EXPANDER(frame), FALSE);
    fLoggingBox = GTK_EXPANDER(frame);

    // create text buffer
    GtkTextBuffer* buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_set_text(buffer, "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n", -1);

    GtkWidget* tbox = gtk_text_view_new_with_buffer(buffer);
    gtk_container_set_border_width (GTK_CONTAINER (tbox), 0);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tbox), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tbox), FALSE);
    gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW(tbox), 0);
    gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW(tbox), 2);
    gtk_text_view_set_justification(GTK_TEXT_VIEW(tbox), GTK_JUSTIFY_LEFT);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tbox), 5);
    gtk_text_view_set_indent(GTK_TEXT_VIEW(tbox), 0);

    gtk_container_add (GTK_CONTAINER(box), scrollbox);
    gtk_container_add(GTK_CONTAINER(scrollbox),GTK_WIDGET(tbox));
    gtk_widget_show(tbox);
    gtk_widget_show(scrollbox);
    fLoggingWindow = GTK_TEXT_VIEW(tbox);

    gtk_widget_show(box);
  }

  void GxMainInterface::openSignalLevelBox(const char* label)
  {

    GtkWidget* hbox = addWidget(label, gtk_hbox_new (FALSE, 0));

    gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);
    gtk_box_set_spacing(GTK_BOX(hbox), 3);
    gtk_widget_set_size_request (GTK_WIDGET(hbox), 20.0, 145.0);
    g_signal_connect(hbox, "expose-event", G_CALLBACK(box7_expose), NULL);
    g_signal_connect(GTK_CONTAINER(hbox), "check-resize",
		     G_CALLBACK(box7_expose), NULL);


    // guitarix output levels
    GtkWidget* lvl = gtk_level_bar_new(47, 1, 2);
    gtk_box_pack_start(GTK_BOX(hbox), lvl, FALSE, FALSE, 0);
    GtkTooltips *comandline = gtk_tooltips_new ();
    gtk_tooltips_set_tip(comandline,
                       lvl, "guitarix engine output", " ");


    gtk_widget_show(lvl);
    fSignalLevelBar = lvl;

    /* jconv levels */
    lvl = gtk_level_bar_new(47, 1, 2);
    gtk_box_pack_end(GTK_BOX(hbox), lvl, FALSE, FALSE, 0);
    gtk_widget_hide(lvl);
    fJCSignalLevelBar = lvl;
    GtkTooltips *comandline1 = gtk_tooltips_new ();
    gtk_tooltips_set_tip(comandline1,
                       lvl, "jconv output", " ");

    gtk_widget_show(hbox);

  }

  /* --- create the portmap window with tabbed client port tables --- */
  void GxMainInterface::createPortMapWindow(const char* label)
  {
    // static box containing all
    GtkWidget* vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
    g_signal_connect(vbox, "expose-event", G_CALLBACK(box4_expose), NULL);
    gtk_widget_show(vbox);

    // static hbox containing guitarix port names
    GtkWidget* hbox = gtk_hbox_new(FALSE, 2);
    for (int i = gx_jack::kAudioInput; i <= gx_jack::kAudioOutput2; i++)
      {
	string pname =
	  gx_jack::client_name + string(" : ") +
	  gx_jack::gx_port_names[i];

	GtkWidget* label = gtk_label_new(pname.c_str());
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 2);
	gtk_widget_show(label);
      }


    gtk_widget_show(hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox,   FALSE, FALSE, 2);

    // add seperator
    GtkWidget* sep = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);
    gtk_widget_show(sep);

    // notebook
    GtkWidget* nb = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(nb), TRUE);

    fPortMapTabs = GTK_NOTEBOOK(nb);

    // scrolled window
    GtkWidget* scrlwd = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrlwd),
				   GTK_POLICY_AUTOMATIC,GTK_POLICY_ALWAYS);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrlwd),
					GTK_SHADOW_IN);

    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrlwd), nb);
    gtk_widget_show(nb);
    gtk_widget_show(scrlwd);

    // add scrolled window in vbox
    gtk_box_pack_start(GTK_BOX(vbox), scrlwd, TRUE, TRUE, 2);

    // main window
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW (window), label);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(fWindow));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);
    gtk_window_add_accel_group(GTK_WINDOW(window), fAccelGroup);
    gtk_window_set_gravity(GTK_WINDOW(window), GDK_GRAVITY_NORTH_WEST);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_hide(window);

    fPortMapWindow = GTK_WINDOW(window);
  }

  void GxMainInterface::openHorizontalBox(const char* label)
  {
    GtkWidget * box = gtk_hbox_new (homogene, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 0);

    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	GtkWidget * frame = addWidget(label, gtk_frame_new (label));
	gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER(frame), box);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  void GxMainInterface::openHandleBox(const char* label)
  {
    GtkWidget * box = gtk_hbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);
    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	GtkWidget * frame = addWidget(label, gtk_handle_box_new ());
	gtk_container_add (GTK_CONTAINER(frame), box);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  void GxMainInterface::openEventBox(const char* label)
  {
    GtkWidget * box = gtk_hbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);
    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	GtkWidget * frame = addWidget(label, gtk_event_box_new ());
	gtk_container_add (GTK_CONTAINER(frame), box);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  struct uiExpanderBox : public gx_ui::GxUiItem
  {
    GtkExpander* fButton;
    uiExpanderBox(gx_ui::GxUI* ui, float* zone, GtkExpander* b) : gx_ui::GxUiItem(ui, zone), fButton(b) {}
    static void expanded (GtkWidget *widget, gpointer data)
    {
      float v = gtk_expander_get_expanded  (GTK_EXPANDER(widget));

      if (v == 1.000000)
	v = 0;
      else v = 1;

      ((gx_ui::GxUiItem*)data)->modifyZone(v);
    }

    virtual void reflectZone()
    {
      float 	v = *fZone;
      fCache = v;
      gtk_expander_set_expanded(GTK_EXPANDER(fButton), v);
    }
  };

  void GxMainInterface::openExpanderBox(const char* label, float* zone)
  {
    *zone = 0.0;
    GtkWidget * box = gtk_hbox_new (homogene, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 0);
    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	GtkWidget * frame = addWidget(label, gtk_expander_new (label));
	gtk_container_add (GTK_CONTAINER(frame), box);
	uiExpanderBox* c = new uiExpanderBox(this, zone, GTK_EXPANDER(frame));
	g_signal_connect (GTK_OBJECT (frame), "activate", G_CALLBACK (uiExpanderBox::expanded), (gpointer)c);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  void GxMainInterface::openVerticalBox(const char* label)
  {
    GtkWidget * box = gtk_vbox_new (homogene, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 0);
    g_signal_connect(box, "expose-event", G_CALLBACK(box10_expose), NULL);

    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	// GtkWidget * frame = addWidget(label, gtk_frame_new (label));
	// gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_NONE);
	GtkWidget* lw = gtk_label_new(label);
	GdkColor colorGreen;
	gdk_color_parse("#a6a9aa", &colorGreen);
	gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
	GtkStyle *style = gtk_widget_get_style(lw);
	pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
	pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
	gtk_widget_modify_font(lw, style->font_desc);
	//  g_signal_connect(box, "expose-event", G_CALLBACK(box_expose), NULL);

	gtk_container_add (GTK_CONTAINER(box), lw);
	gtk_box_pack_start (GTK_BOX(fBox[fTop]), box, expand, fill, 0);
	gtk_widget_show(lw);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  void GxMainInterface::openPaintBox(const char* label)
  {
    GtkWidget * box = gtk_vbox_new (homogene, 2);
    gtk_container_set_border_width (GTK_CONTAINER (box), 4);
    g_signal_connect(box, "expose-event", G_CALLBACK(box_expose), NULL);

    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	gtk_box_pack_start (GTK_BOX(fBox[fTop]), box, expand, fill, 0);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  void GxMainInterface::openPaintBox2(const char* label)
  {
    GtkWidget * box = gtk_vbox_new (homogene, 2);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);
    g_signal_connect(box, "expose-event", G_CALLBACK(box2_expose), NULL);

    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	gtk_box_pack_start (GTK_BOX(fBox[fTop]), box, expand, fill, 0);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  void GxMainInterface::openVerticalBox1(const char* label)
  {
    GtkWidget * box = gtk_vbox_new (homogene, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 0);
    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	// GtkWidget * frame = addWidget(label, gtk_frame_new (label));
	// gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_NONE);
	GtkWidget* lw = gtk_label_new(label);
	GdkColor colorGreen;
	gdk_color_parse("#a6a9aa", &colorGreen);
	gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
	GtkStyle *style = gtk_widget_get_style(lw);
	pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
	pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
	gtk_widget_modify_font(lw, style->font_desc);
	gtk_container_add (GTK_CONTAINER(box), lw);
	gtk_box_pack_start (GTK_BOX(fBox[fTop]), box, expand, fill, 0);
	gtk_widget_show(lw);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  void GxMainInterface::openVerticalMidiBox(const char* label)
  {
    midibox = gtk_vbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (midibox), 2);
    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	GtkWidget * frame = addWidget(label, gtk_frame_new (label));
	gtk_container_add (GTK_CONTAINER(frame), midibox);
	gtk_widget_show(midibox);
	pushBox(kBoxMode, midibox);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, midibox));
      }
    gtk_widget_hide(midibox);
  }

  void GxMainInterface::openToolBar(const char* label)
  {
    GtkWidget * box = gtk_toolbar_new  ();


    if (fMode[fTop] != kTabMode && label[0] != 0)
      {
	// GtkWidget * frame = addWidget(label, gtk_frame_new (label));
	// gtk_frame_set_shadow_type(GTK_FRAME(frame),GTK_SHADOW_NONE);
	GtkWidget* lw = gtk_label_new(label);
	GdkColor colorGreen;
	gdk_color_parse("#a6a9aa", &colorGreen);
	gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
	GtkStyle *style = gtk_widget_get_style(lw);
	pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
	pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
	gtk_widget_modify_font(lw, style->font_desc);
	//  g_signal_connect(box, "expose-event", G_CALLBACK(box_expose), NULL);

	gtk_container_add (GTK_CONTAINER(box), lw);
	gtk_box_pack_start (GTK_BOX(fBox[fTop]), box, expand, fill, 0);
	gtk_widget_show(lw);
	gtk_widget_show(box);
	pushBox(kBoxMode, box);
      }
    else
      {
	pushBox(kBoxMode, addWidget(label, box));
      }
  }

  void GxMainInterface::openScrollBox(const char* label)
  {
    GtkWidget * scrollbox = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrollbox),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    // gtk_widget_set_size_request (scrollbox, 580, 250);
    GtkWidget * box = gtk_vbox_new (homogene, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box), 0);
    GtkWidget * frame =  gtk_handle_box_new ();
    gtk_handle_box_set_handle_position(GTK_HANDLE_BOX(frame),GTK_POS_TOP);
    gtk_container_add (GTK_CONTAINER(frame), box);
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollbox),GTK_WIDGET(frame));

    gtk_container_add (GTK_CONTAINER(fBox[fTop]), scrollbox);
    gtk_widget_show_all(scrollbox);
    pushBox(kBoxMode, box);

  }

  GtkWidget* GxMainInterface::addWidget(const char* label, GtkWidget* w)
  {
    switch (fMode[fTop])
      {
      case kSingleMode	:
	gtk_container_add (GTK_CONTAINER(fBox[fTop]), w);
	break;
      case kBoxMode 		:
	gtk_box_pack_start (GTK_BOX(fBox[fTop]), w, expand, fill, 0);
	break;
      case kTabMode 		:
	gtk_notebook_append_page (GTK_NOTEBOOK(fBox[fTop]), w, gtk_label_new(label));
	break;
      }
    gtk_widget_show (w);
    return w;
  }

  // --------------------------- Press button ---------------------------

  struct uiButton : public gx_ui::GxUiItem
  {
    GtkButton* 	fButton;
    uiButton (gx_ui::GxUI* ui, float* zone, GtkButton* b) : gx_ui::GxUiItem(ui, zone), fButton(b) {}
    static void pressed( GtkWidget *widget, gpointer   data )
    {
      gx_ui::GxUiItem* c = (gx_ui::GxUiItem*) data;
      c->modifyZone(1.0);
    }

    static void released( GtkWidget *widget, gpointer   data )
    {
      gx_ui::GxUiItem* c = (gx_ui::GxUiItem*) data;
      c->modifyZone(0.0);
    }

    virtual void reflectZone()
    {
      float 	v = *fZone;
      fCache = v;
      if (v > 0.0) gtk_button_pressed(fButton);
      else gtk_button_released(fButton);
    }
  };

  void GxMainInterface::addJConvButton(const char* label, float* zone)
  {
    *zone = 0.0;
    GtkWidget* 	button = gtk_button_new_with_label (label);
    addWidget(label, button);

    uiButton* c = new uiButton(this, zone, GTK_BUTTON(button));

    g_signal_connect (GTK_OBJECT (button), "pressed",
		      G_CALLBACK (uiButton::pressed), (gpointer) c);

    g_signal_connect (GTK_OBJECT (button), "released",
		      G_CALLBACK (uiButton::released), (gpointer) c);

    g_signal_connect (GTK_OBJECT (button), "clicked",
		      G_CALLBACK (gx_jconv::gx_setting_jconv_dialog_gui),
		      button);
  }

  // ---------------------------	Toggle Buttons ---------------------------

  struct uiToggleButton : public gx_ui::GxUiItem
  {
    GtkToggleButton* fButton;
    uiToggleButton(gx_ui::GxUI* ui, float* zone, GtkToggleButton* b) : gx_ui::GxUiItem(ui, zone), fButton(b) {}
    static void toggled (GtkWidget *widget, gpointer data)
    {
      float	v = (GTK_TOGGLE_BUTTON (widget)->active) ? 1.0 : 0.0;
      ((gx_ui::GxUiItem*)data)->modifyZone(v);
    }

    virtual void reflectZone()
    {
      float v = *fZone;
      fCache = v;
      gtk_toggle_button_set_active(fButton, v > 0.0);
    }
  };

  void GxMainInterface::addToggleButton(const char* label, float* zone)
  {
    GdkColor colorRed;
    GdkColor colorOwn;
    gdk_color_parse ("#d56161", &colorRed);
    gdk_color_parse ("#7f7f7f", &colorOwn);
    float local_zone = 0.0;
    if (zone) local_zone = *zone;
    GtkWidget* 	button = gtk_toggle_button_new_with_label (label);
    addWidget(label, button);

    gtk_widget_modify_bg (button, GTK_STATE_NORMAL, &colorOwn);
    gtk_widget_modify_bg (button, GTK_STATE_ACTIVE, &colorRed);

    g_signal_connect (GTK_OBJECT (button), "toggled", G_CALLBACK (gx_start_stop_jack_capture), NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
    gtk_widget_add_accelerator(button, "activate", fAccelGroup, GDK_r, GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);

    record_button = button;
  }

  void GxMainInterface::addPToggleButton(const char* label, float* zone)
  {
    GdkColor colorRed;
    GdkColor colorOwn;
    GdkColor colorwn;
    gdk_color_parse ("#58b45e", &colorRed);
    gdk_color_parse ("#000000", &colorwn);
    gdk_color_parse ("#7f7f7f", &colorOwn);
    *zone = 0.0;
    GtkWidget* 	button = gtk_toggle_button_new();
    GtkWidget* 	lab = gtk_label_new(label);
    GtkStyle *style = gtk_widget_get_style(lab);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
    gtk_widget_modify_font(lab, style->font_desc);
    gtk_container_add (GTK_CONTAINER(button), lab);
    gtk_widget_set_size_request (GTK_WIDGET(button), 60.0, 20.0);
    GtkWidget * box = gtk_hbox_new (homogene, 4);
    GtkWidget * box1 = gtk_vbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (box), 0);
    gtk_container_set_border_width (GTK_CONTAINER (box1), 0);
    gtk_container_add (GTK_CONTAINER(box), box1);
    GtkWidget * box2 = gtk_vbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (box2), 0);
    gtk_widget_set_size_request (GTK_WIDGET(box2), 6.0, 20.0);
    gtk_container_add (GTK_CONTAINER(box), button);
    gtk_container_add (GTK_CONTAINER(box), box2);
    gtk_widget_set_size_request (GTK_WIDGET(box1), 6.0, 20.0);
    gtk_widget_show (button);
    gtk_widget_show (box1);
    gtk_widget_show (box2);
    gtk_widget_show (lab);
    gtk_widget_show (box);
    gtk_container_add (GTK_CONTAINER(fBox[fTop]), box);
    uiToggleButton* c = new uiToggleButton(this, zone, GTK_TOGGLE_BUTTON(button));
    gtk_widget_modify_bg (button, GTK_STATE_NORMAL, &colorOwn);
    gtk_widget_modify_bg (button, GTK_STATE_ACTIVE, &colorRed);
    gtk_widget_modify_fg (lab, GTK_STATE_ACTIVE, &colorwn);
    g_signal_connect (GTK_OBJECT (button), "toggled", G_CALLBACK (uiToggleButton::toggled), (gpointer) c);
  }

  void GxMainInterface::addJToggleButton(const char* label, float* zone)
  {
    GdkColor colorRed;
    GdkColor colorOwn;
    GdkColor colorwn;

    gdk_color_parse ("#58b45e", &colorRed);
    gdk_color_parse ("#000000", &colorwn);
    gdk_color_parse ("#7f7f7f", &colorOwn);

    *zone = 0.0;

    GtkWidget* 	button = gtk_toggle_button_new();
    GtkWidget* 	lab = gtk_label_new(label);
    GtkStyle *style = gtk_widget_get_style(lab);

    pango_font_description_set_size(style->font_desc, 10*PANGO_SCALE);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
    gtk_widget_modify_font(lab, style->font_desc);
    gtk_container_add (GTK_CONTAINER(button), lab);
    addWidget(label, button);
    gtk_widget_show (lab);

    uiToggleButton* c =
      new uiToggleButton(this, zone, GTK_TOGGLE_BUTTON(button));

    gtk_widget_modify_bg (button, GTK_STATE_NORMAL, &colorOwn);
    gtk_widget_modify_bg (button, GTK_STATE_ACTIVE, &colorRed);
    gtk_widget_modify_fg (lab, GTK_STATE_ACTIVE, &colorwn);

    g_signal_connect (GTK_OBJECT (button), "toggled",
		      G_CALLBACK (uiToggleButton::toggled), (gpointer) c);

    g_signal_connect (GTK_OBJECT (button), "toggled",
		      G_CALLBACK (gx_child_process::gx_start_stop_jconv), (gpointer)c);
  }

  // ---------------------------	Check Button ---------------------------

  struct uiCheckButton : public gx_ui::GxUiItem
  {
    GtkToggleButton* fButton;
    uiCheckButton(gx_ui::GxUI* ui, float* zone, GtkToggleButton* b) : gx_ui::GxUiItem(ui, zone), fButton(b) {}
    static void toggled (GtkWidget *widget, gpointer data)
    {
      float	v = (GTK_TOGGLE_BUTTON (widget)->active) ? 1.0 : 0.0;
      ((gx_ui::GxUiItem*)data)->modifyZone(v);
    }

    virtual void reflectZone()
    {
      float 	v = *fZone;
      fCache = v;
      gtk_toggle_button_set_active(fButton, v > 0.0);
    }
  };

  void GxMainInterface::addCheckButton(const char* label, float* zone)
  {
    GdkColor   colorRed;
    GdkColor   colorOwn;
    GdkColor   colorba;
    gdk_color_parse ("#000000", &colorRed);
    gdk_color_parse ("#4c5159", &colorOwn);
    gdk_color_parse ("#c4c0c0", &colorba);
    GtkWidget* 	lab = gtk_label_new(label);
    GtkWidget* 	button = gtk_check_button_new ();
    gtk_container_add (GTK_CONTAINER(button), lab);
    addWidget(label, button);
    gtk_widget_modify_bg (button, GTK_STATE_PRELIGHT, &colorOwn);
    gtk_widget_modify_fg (button, GTK_STATE_PRELIGHT, &colorRed);
    gtk_widget_modify_text (button, GTK_STATE_NORMAL, &colorRed);
    gtk_widget_modify_base (button, GTK_STATE_NORMAL, &colorba);
    GtkStyle *style = gtk_widget_get_style(lab);
    pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
    gtk_widget_modify_font(lab, style->font_desc);
    uiCheckButton* c = new uiCheckButton(this, zone, GTK_TOGGLE_BUTTON(button));
    g_signal_connect (GTK_OBJECT (button), "toggled", G_CALLBACK(uiCheckButton::toggled), (gpointer) c);
    gtk_widget_show (lab);
  }

  // ---------------------------	Adjustmenty based widgets ---------------------------

  struct uiAdjustment : public gx_ui::GxUiItem
  {
    GtkAdjustment* fAdj;
    uiAdjustment(gx_ui::GxUI* ui, float* zone, GtkAdjustment* adj) : gx_ui::GxUiItem(ui, zone), fAdj(adj) {}
    static void changed (GtkWidget *widget, gpointer data)
    {
      float	v = GTK_ADJUSTMENT (widget)->value;
      ((gx_ui::GxUiItem*)data)->modifyZone(v);
    }

    virtual void reflectZone()
    {
      float 	v = *fZone;
      fCache = v;
      gtk_adjustment_set_value(fAdj, v);
    }
  };

  int precision(double n)
  {
    if (n < 0.009999) return 3;
    else if (n < 0.099999) return 2;
    else if (n < 0.999999) return 1;
    else return 0;
  }

  // -------------------------- Vertical Slider -----------------------------------

  void GxMainInterface::addVerticalSlider(const char* label, float* zone, float init, float min, float max, float step)
  {
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GdkColor colorRed;
    GdkColor colorGreen;
    GdkColor colorYellow;
    GtkWidget* slider = gtk_vscale_new (GTK_ADJUSTMENT(adj));
    gdk_color_parse("#c4c0c0", &colorGreen);
    gdk_color_parse("#96a2a7", &colorYellow);
    gdk_color_parse("#7b8a90", &colorRed);
    gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
    gtk_scale_set_digits(GTK_SCALE(slider), precision(step));
    gtk_widget_set_usize(slider, -1, 120);
    gtk_widget_modify_bg (slider, GTK_STATE_NORMAL, &colorRed);
    gtk_widget_modify_bg (slider, GTK_STATE_PRELIGHT, &colorYellow);
    gtk_widget_modify_bg(slider, GTK_STATE_ACTIVE, &colorYellow);
    gtk_widget_modify_bg(slider, GTK_STATE_SELECTED, &colorGreen);
    openFrameBox(label);
    addWidget(label, slider);
    closeBox();
  }

  // -------------------------- Horizontal Slider -----------------------------------


  void GxMainInterface::addHorizontalSlider(const char* label, float* zone, float init, float min, float max, float step)
  {
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_mini_slider_new_with_adjustment (GTK_ADJUSTMENT(adj));
    gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
    addWidget(label, slider);
  }

  void GxMainInterface::addHorizontalWheel(const char* label, float* zone, float init, float min, float max, float step)
  {
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_wheel_new_with_adjustment (GTK_ADJUSTMENT(adj));
    gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
    addWidget(label, slider);
  }

  struct uiValueDisplay : public gx_ui::GxUiItem
  {
    GtkLabel* fLabel;
    int	fPrecision ;

    uiValueDisplay(gx_ui::GxUI* ui, float* zone, GtkLabel* label, int precision)
      : gx_ui::GxUiItem(ui, zone), fLabel(label), fPrecision(precision) {}

    virtual void reflectZone()
    {
      float v = *fZone;
      fCache = v;
      char s[64];
      if (fPrecision <= 0)
	snprintf(s, 63, "%d", int(v));

      else if (fPrecision > 3)
	snprintf(s, 63, "%f", v);

      else if (fPrecision == 1)
	{
	  const char* format[] = {"%.1f", "%.2f", "%.3f"};
	  snprintf(s, 63, format[1-1], v);
	}
      else if (fPrecision == 2)
	{
	  const char* format[] = {"%.1f", "%.2f", "%.3f"};
	  snprintf(s, 63, format[2-1], v);
	}
      else
	{
	  const char* format[] = {"%.1f", "%.2f", "%.3f"};
	  snprintf(s, 63, format[3-1], v);
	}
      gtk_label_set_text(fLabel, s);
    }
  };


  void GxMainInterface::addregler(const char* label, float* zone, float init, float min, float max, float step)
  {
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkWidget* lw = gtk_label_new("");
    GtkWidget* lwl = gtk_label_new(label);
    GdkColor colorGreen;
    gdk_color_parse("#a6a9aa", &colorGreen);
    gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
    GtkStyle *style = gtk_widget_get_style(lw);
    pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
    gtk_widget_modify_font(lw, style->font_desc);
    gtk_widget_modify_font(lwl, style->font_desc);
    new uiValueDisplay(this, zone, GTK_LABEL(lw),precision(step));
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_regler_new_with_adjustment(GTK_ADJUSTMENT(adj));
    gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
    openVerticalBox("");
    addWidget(label, lwl);
    addWidget(label, slider);
    addWidget(label, lw);
    closeBox();
  }

  void GxMainInterface::addbigregler(const char* label, float* zone, float init, float min, float max, float step)
  {
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkWidget* lw = gtk_label_new("");
    GtkWidget* lwl = gtk_label_new(label);
    GdkColor colorGreen;
    gdk_color_parse("#a6a9aa", &colorGreen);
    gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
    GtkStyle *style = gtk_widget_get_style(lw);
    pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
    gtk_widget_modify_font(lw, style->font_desc);
    gtk_widget_modify_font(lwl, style->font_desc);

    new uiValueDisplay(this, zone, GTK_LABEL(lw),precision(step));
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_big_regler_new_with_adjustment(GTK_ADJUSTMENT(adj));
    gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
    openVerticalBox("");
    addWidget(label, lwl);
    addWidget(label, slider);
    addWidget(label, lw);
    closeBox();
  }

  void GxMainInterface::addslider(const char* label, float* zone, float init, float min, float max, float step)
  {
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkWidget* lw = gtk_label_new("");
    GdkColor colorGreen;
    gdk_color_parse("#a6a9aa", &colorGreen);
    gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
    GtkStyle *style = gtk_widget_get_style(lw);
    pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
    gtk_widget_modify_font(lw, style->font_desc);
    new uiValueDisplay(this, zone, GTK_LABEL(lw),precision(step));
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_hslider_new_with_adjustment(GTK_ADJUSTMENT(adj));
    gtk_range_set_inverted (GTK_RANGE(slider), TRUE);
    openVerticalBox(label);
    addWidget(label, slider);
    addWidget(label, lw);

    closeBox();
  }

  void GxMainInterface::addtoggle(const char* label, float* zone)
  {
    GtkObject* adj = gtk_adjustment_new(0, 0, 1, 1, 10*1, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_toggle_new_with_adjustment(GTK_ADJUSTMENT(adj));
    addWidget(label, slider);
  }

  void GxMainInterface::addbtoggle(const char* label, float* zone)
  {
    GtkObject* adj = gtk_adjustment_new(0, 0, 1, 1, 10*1, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_button_toggle_new_with_adjustment(GTK_ADJUSTMENT(adj));
    addWidget(label, slider);
  }

  void GxMainInterface::addswitch(const char* label, float* zone)
  {
    GtkObject* adj = gtk_adjustment_new(0, 0, 1, 1, 10*1, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_switch_new_with_adjustment(GTK_ADJUSTMENT(adj));
    GtkWidget* lw = gtk_label_new(label);
    GdkColor colorGreen;
    gdk_color_parse("#a6a9aa", &colorGreen);
    gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
    GtkStyle *style = gtk_widget_get_style(lw);
    pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
    gtk_widget_modify_font(lw, style->font_desc);
    openVerticalBox("");
    string laba = label;
    if (laba !="")addWidget(label, lw);
    addWidget(label, slider);
    closeBox();
  }

  void GxMainInterface::addminiswitch(const char* label, float* zone)
  {
    GtkObject* adj = gtk_adjustment_new(0, 0, 1, 1, 10*1, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkRegler myGtkRegler;
    GtkWidget* slider = myGtkRegler.gtk_mini_toggle_new_with_adjustment(GTK_ADJUSTMENT(adj));
    GtkWidget* lw = gtk_label_new(label);
    GdkColor colorGreen;
    gdk_color_parse("#a6a9aa", &colorGreen);
    gtk_widget_modify_fg (lw, GTK_STATE_NORMAL, &colorGreen);
    GtkStyle *style = gtk_widget_get_style(lw);
    pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_LIGHT);
    gtk_widget_modify_font(lw, style->font_desc);
    openHorizontalBox("");
    addWidget(label, slider);
    addWidget(label, lw);
    closeBox();
  }

  void GxMainInterface::addNumEntry(const char* label, float* zone, float init, float min, float max, float step)
  {
    *zone = init;
    GtkObject* adj = gtk_adjustment_new(init, min, max, step, 10*step, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkWidget* spinner = gtk_spin_button_new (GTK_ADJUSTMENT(adj), step, precision(step));
    openFrameBox(label);
    addWidget(label, spinner);
    closeBox();
  }

  void GxMainInterface::openWarningBox(const char* label, float* zone)
  {
    GtkWidget* 	button = gtk_check_button_new ();
    uiCheckButton* c = new uiCheckButton(this, zone, GTK_TOGGLE_BUTTON(button));
    g_signal_connect (GTK_OBJECT (button), "toggled", G_CALLBACK(uiCheckButton::toggled), (gpointer) c);
  }

  void GxMainInterface::setSkinBox(const char* label, float* zone)
  {
    GtkObject* adj = gtk_adjustment_new(99, 0, 100, 1, 10*1, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));

    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (gx_set_skin),  (gpointer) c);
  }

  void GxMainInterface::openDialogBox(const char* label, float* zone)
  {
    GtkWidget * dialog = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(dialog), TRUE);
    // gtk_window_set_deletable(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_gravity(GTK_WINDOW(dialog), GDK_GRAVITY_SOUTH);
    gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(fWindow));
    gtk_window_set_position (GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_keep_below (GTK_WINDOW(dialog), FALSE);
    gtk_window_set_title (GTK_WINDOW (dialog), label);

    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    GtkWidget * box = gtk_hbox_new (homogene, 4);
    GtkWidget * box4 = gtk_vbox_new (homogene, 4);
    GtkWidget * box5 = gtk_hbox_new (homogene, 4);
    gtk_container_set_border_width (GTK_CONTAINER (box), 2);
    GdkColor colorRed;
    GdkColor colorOwn;
    gdk_color_parse ("#000094", &colorRed);
    gdk_color_parse ("#7f7f7f", &colorOwn);
    *zone = 0.0;

    GtkObject* adj = gtk_adjustment_new(0, 0, 1, 1, 10*1, 0);
    uiAdjustment* c = new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));
    g_signal_connect (GTK_OBJECT (adj), "value-changed", G_CALLBACK (uiAdjustment::changed), (gpointer) c);
    GtkRegler myGtkRegler;
    GtkWidget* button = myGtkRegler.gtk_button_toggle_new_with_adjustment(GTK_ADJUSTMENT(adj));

    GtkWidget * box3 = gtk_hbox_new (homogene, 0);
    gtk_container_set_border_width (GTK_CONTAINER (box3), 0);
    gtk_container_add (GTK_CONTAINER(box3), button);
    gtk_widget_show (button);
    gtk_widget_show (box3);
    gtk_container_add (GTK_CONTAINER(fBox[fTop]), box3);
    gtk_widget_modify_bg (button, GTK_STATE_NORMAL, &colorOwn);
    gtk_widget_modify_bg (button, GTK_STATE_ACTIVE, &colorRed);
    g_signal_connect (GTK_OBJECT (button), "value-changed", G_CALLBACK (gx_show_extended_settings), (gpointer) dialog);
    g_signal_connect_swapped (G_OBJECT (dialog), "delete_event", G_CALLBACK (gx_delete_event), (gpointer) button);
    GtkWidget * frame =  gtk_frame_new (label);
    GtkWidget* 	lab = gtk_label_new("reset");
    GtkWidget* 	button1 = gtk_button_new();
    gtk_container_add (GTK_CONTAINER(button1), lab);

    GtkStyle *style = gtk_widget_get_style(lab);
    pango_font_description_set_size(style->font_desc, 10*PANGO_SCALE);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_NORMAL);
    gtk_widget_modify_font(lab, style->font_desc);

    gtk_container_add (GTK_CONTAINER(box5), frame);
    gtk_container_add (GTK_CONTAINER(box5), button1);
    g_signal_connect  (GTK_OBJECT (button1), "pressed", G_CALLBACK (gx_reset_units), (gpointer) dialog);
    gtk_container_add (GTK_CONTAINER(box4), box5);
    gtk_container_add (GTK_CONTAINER(box4), box);
    gtk_container_add (GTK_CONTAINER(dialog), box4);
    // gtk_widget_show(dialog);
    gtk_widget_show(lab);
    gtk_widget_show(frame);
    gtk_widget_show(button1);
    gtk_widget_show(box);
    gtk_widget_show(box4);
    gtk_widget_show(box5);
    pushBox(kBoxMode, box);
  }

  // ------------------------------ Num Display -----------------------------------

  struct uiNumDisplay : public gx_ui::GxUiItem
  {
    GtkWidget* fLabel;

    uiNumDisplay(gx_ui::GxUI* ui, float* zone, GtkWidget* label)
      : gx_ui::GxUiItem(ui, zone), fLabel(label) {}

    virtual void reflectZone()
    {
      float v = *fZone;
      fCache = v;
      char s[64];
      int vis = round(v);
      float scale = ((v-vis)-(-1.0))/(1.0-(-1.0));
      if ((scale <= 0.0) || (scale > 1.0)) scale = 0.0;
      vis += 9;
      const char* note[] = {"C ","C#","D ","D#","E ","F ","F#","G ","G#","A ","A#","B "};
      if (shownote == 1)
	{
	  if ((vis>=0)&&(vis<=11)) snprintf(s, 63, "%s",  note[vis]);
	  else if ((vis>=-24)&&(vis<=-13)) snprintf(s, 63, "%s", note[vis+24]);
	  else if ((vis>=-12)&&(vis<=-1)) snprintf(s, 63, "%s", note[vis+12]);
	  else if ((vis>=12)&&(vis<=23)) snprintf(s, 63, "%s", note[vis-12]);
	  else if ((vis>=24)&&(vis<=35)) snprintf(s, 63,"%s", note[vis-24]);
	  else if ((vis>=36)&&(vis<=47)) snprintf(s, 63,"%s", note[vis-36]);
	  else
	    {
	      snprintf(s, 63, "%s", "");
	      scale = 0.0;
	    }
	  if ((scale >= 0.0) && (scale < 1.0)) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pb), scale);
	  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(pb), s);
	}
      else if (shownote == 0)
	{
	  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pb), scale);
	  snprintf(s, 63, "%s", "");
	  gtk_progress_bar_set_text(GTK_PROGRESS_BAR(pb), s);
	  shownote = -1;
	}
    }
  };


  void GxMainInterface::addNumDisplay(const char* label, float* zone )
  {
    openVerticalBox(label);
    pb = gtk_progress_bar_new();
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(pb), GTK_PROGRESS_LEFT_TO_RIGHT);
    new uiNumDisplay(this, zone, GTK_WIDGET(pb));
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(pb), label);
    gtk_widget_set_size_request(pb, 40.0, 20.0);
    GtkStyle *style = gtk_widget_get_style(pb);
    pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
    gtk_widget_modify_font(pb, style->font_desc);
    addWidget(label, pb);
    gtk_widget_hide(pb);
    closeBox();
  }

  struct uiStatusDisplay : public gx_ui::GxUiItem
  {
    GtkLabel* fLabel;
    int	fPrecision;

    uiStatusDisplay(gx_ui::GxUI* ui, float* zone, GtkLabel* label)
      : gx_ui::GxUiItem(ui, zone), fLabel(label) {}

    virtual void reflectZone()
    {
      float 	v = *fZone;
      fCache = v;
      if ((gx_engine::dsp::isMidiOn() == true) &&
	  (gx_jack::jcpu_load < 65.0))
	{
	  if (v > 0.0f) gtk_status_icon_set_from_pixbuf ( GTK_STATUS_ICON(status_icon), GDK_PIXBUF(ibm));
	  else  gtk_status_icon_set_from_pixbuf ( GTK_STATUS_ICON(status_icon), GDK_PIXBUF(ib));
	}
      else if (gx_engine::dsp::isMidiOn() == false)
	{
	  gtk_status_icon_set_from_pixbuf ( GTK_STATUS_ICON(status_icon), GDK_PIXBUF(ib));
	}
      else gtk_status_icon_set_from_pixbuf ( GTK_STATUS_ICON(status_icon), GDK_PIXBUF(ibr));
    }
  };

  void GxMainInterface::addStatusDisplay(const char* label, float* zone )
  {
    GtkWidget* lw = gtk_label_new("");
    new uiStatusDisplay(this, zone, GTK_LABEL(lw));
    openFrameBox(label);
    addWidget(label, lw);
    closeBox();
    gtk_widget_hide(lw);
  };

  void GxMainInterface::addLiveWaveDisplay(const char* label, float* zone , float* zone1)
  {
    GtkObject* adj = gtk_adjustment_new(0.0, -1.0, 1.0, 0.0009, 10*0.0009, 0);
    new uiAdjustment(this, zone, GTK_ADJUSTMENT(adj));

    livewa = gx_wave_live_view(zone,zone1,GTK_ADJUSTMENT(adj));

    //GtkWidget * nolivewa = gtk_event_box_new ();
    GtkWidget * box      = gtk_vbox_new (false, 4);
    g_signal_connect(box, "expose-event", G_CALLBACK(box1_expose), NULL);

    //gtk_widget_set_size_request (nolivewa, 550, 80);
    gtk_widget_set_size_request (box, 552, 82);
    // gtk_container_add (GTK_CONTAINER(nolivewa),box );
    // openPaintBox("");
    gtk_container_add (GTK_CONTAINER(box),livewa );
    addWidget(label, box);
    //closeBox();
    //addWidget(label, nolivewa);

    gtk_widget_show(box);
    gtk_widget_hide(livewa);
    // gtk_widget_hide(nolivewa);
  };


  // ------------------------------ Progress Bar -----------------------------------

  struct uiBargraph : public gx_ui::GxUiItem
  {
    GtkProgressBar*		fProgressBar;
    float				fMin;
    float				fMax;

    uiBargraph(gx_ui::GxUI* ui, float* zone, GtkProgressBar* pbar, float lo, float hi)
      : gx_ui::GxUiItem(ui, zone), fProgressBar(pbar), fMin(lo), fMax(hi) {}

    float scale(float v)
    {
      return (v-fMin)/(fMax-fMin);
    }

    virtual void reflectZone()
    {
      float 	v = *fZone;
      fCache = v;
      if (v >= 1.0) v = 1.0f;
      gtk_progress_bar_set_fraction(fProgressBar, scale(v));
    }
  };

  void GxMainInterface::addVerticalBargraph(const char* label, float* zone, float lo, float hi)
  {
    GtkWidget* pb = gtk_progress_bar_new();
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(pb), GTK_PROGRESS_RIGHT_TO_LEFT);
    gtk_widget_set_size_request(pb,  -1,8);
    new uiBargraph(this, zone, GTK_PROGRESS_BAR(pb), lo, hi);
    openFrameBox(label);
    addWidget(label, pb);
    closeBox();
  }

  void GxMainInterface::addHorizontalBargraph(const char* label, float* zone, float lo, float hi)
  {
    GtkWidget* pb = gtk_progress_bar_new();
    gtk_progress_bar_set_orientation(GTK_PROGRESS_BAR(pb), GTK_PROGRESS_LEFT_TO_RIGHT);
    gtk_widget_set_size_request(pb, -1, 8);
    new uiBargraph(this, zone, GTK_PROGRESS_BAR(pb), lo, hi);
    openFrameBox(label);
    addWidget(label, pb);
    closeBox();
  }

  //----------------------------- main menu ----------------------------
  void GxMainInterface::addMainMenu()
  {
    /*-- Declare the GTK Widgets used in the menu --*/
    GtkWidget* menucont;  // menu container
    GtkWidget* hbox;      // top menu bar box container

    /*------------------ TOP Menu BAR ------------------*/
    hbox = gtk_hbox_new(FALSE, 0);

    /*-- Create the menu bar --*/
    menucont = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(hbox), menucont, TRUE, TRUE, 0);

    /*-- Jack server status image --*/
    // jackd ON image
    string img_path = gx_pixmap_dir + "jackd_on.png";

    gx_jackd_on_image = gtk_image_new_from_file(img_path.c_str());
    gtk_box_pack_end(GTK_BOX(hbox), gx_jackd_on_image, FALSE, FALSE, 0);
    gtk_widget_show(gx_jackd_on_image);

    // jackd OFF image: hidden by default
    img_path = gx_pixmap_dir + "jackd_off.png";

    gx_jackd_off_image = gtk_image_new_from_file(img_path.c_str());
    gtk_box_pack_end(GTK_BOX(hbox), gx_jackd_off_image, FALSE, FALSE, 0);
    gtk_widget_hide(gx_jackd_off_image);

    /*-- Engine on/off and status --*/
    // set up ON image: shown by default
    img_path = gx_pixmap_dir + "gx_on.png";

    gx_engine_on_image = gtk_image_new_from_file(img_path.c_str());
    gtk_box_pack_end(GTK_BOX(hbox), gx_engine_on_image, FALSE, TRUE, 0);
    gtk_widget_show(gx_engine_on_image);

    // set up OFF image: hidden by default
    img_path = gx_pixmap_dir + "gx_off.png";

    gx_engine_off_image = gtk_image_new_from_file(img_path.c_str());
    gtk_box_pack_end(GTK_BOX(hbox), gx_engine_off_image, FALSE, TRUE, 0);
    gtk_widget_hide(gx_engine_off_image);

    // set up BYPASS image: hidden by default
    img_path = gx_pixmap_dir + "gx_bypass.png";

    gx_engine_bypass_image = gtk_image_new_from_file(img_path.c_str());
    gtk_box_pack_end(GTK_BOX(hbox), gx_engine_bypass_image, FALSE, TRUE, 0);
    gtk_widget_hide(gx_engine_bypass_image);

    /* ----------------------------------------------------------- */
    fMenuList["Top"] = menucont;

    addEngineMenu();
    addPresetMenu();
    addOptionMenu();
    addAboutMenu();

    /*---------------- add menu to main window box----------------*/
    gtk_box_pack_start (GTK_BOX (fBox[fTop]), hbox , FALSE, FALSE, 0);
    gtk_widget_show(menucont);
    gtk_widget_show(hbox);
  }

  //----------------------------- engine menu ----------------------------
  void GxMainInterface::addEngineMenu()
  {
    GtkWidget* menulabel;   // menu label
    //GtkWidget* menucont;    // menu container use menuh for systray menu here
    GtkWidget* menuitem;    // menu item
    GSList   * group = NULL;

    /*---------------- Create Engine menu items ------------------*/
    menuh = fMenuList["Top"];

    menulabel = gtk_menu_item_new_with_mnemonic ("_Engine");
    gtk_menu_bar_append (GTK_MENU_BAR(menuh), menulabel);
    gtk_widget_show(menulabel);

    /*-- Create Engine submenu  --*/
    menuh = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menuh);
    gtk_widget_show(menuh);
    fMenuList["Engine"] = menuh;

    /*-- Create Engine start / stop item  --*/
    group = NULL;

    menuitem = gtk_check_menu_item_new_with_mnemonic ("Engine _Start / _Stop");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_space, GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);

    gtk_menu_shell_append(GTK_MENU_SHELL(menuh), menuitem);
    gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), TRUE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_engine_switch), (gpointer)0);
    gx_engine_item = menuitem; // save into global var
    gtk_widget_show (menuitem);

    /*-- Create Engine bypass item  --*/
    menuitem = gtk_menu_item_new_with_mnemonic ("Engine _Bypass");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_b, GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);

    gtk_menu_shell_append(GTK_MENU_SHELL(menuh), menuitem);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_engine_switch), (gpointer)1);
    gtk_widget_show (menuitem);

    /*-- add a separator line --*/
    GtkWidget* sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menuh), sep);
    gtk_widget_show (sep);

    /*---------------- Create Jack Server menu --------------------*/
    addJackServerMenu();

    /*---------------- End Jack server menu declarations ----------------*/

    /*-- add a separator line --*/
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menuh), sep);
    gtk_widget_show (sep);

    /*-- Create Open check menu item under Engine submenu --*/
    menuitem = gtk_check_menu_item_new_with_mnemonic ("_Midi Out ");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_m, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_midi_out), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menuh), menuitem);
    gtk_widget_show (menuitem);

    /*-- add a separator line --*/
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menuh), sep);
    gtk_widget_show (sep);

    /*-- Create Exit menu item under Engine submenu --*/
    menuitem = gtk_menu_item_new_with_mnemonic ("_Quit");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect(G_OBJECT (menuitem), "activate",
		     G_CALLBACK (gx_clean_exit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menuh), menuitem);
    gtk_widget_show (menuitem);

    /*---------------- End Engine menu declarations ----------------*/
  }

  //----------------------------- preset menu ----------------------------
  void GxMainInterface::addPresetMenu()
  {
    GtkWidget* menulabel; // menu label
    GtkWidget* menucont;  // menu container
    GtkWidget* menuitem;  // menu item

    menucont = fMenuList["Top"];

    /*---------------- Create Presets menu items --------------------*/
    menulabel = gtk_menu_item_new_with_mnemonic ("_Presets");
    gtk_menu_bar_append (GTK_MENU_BAR(menucont), menulabel);
    gtk_widget_show(menulabel);

    /*-- Create Presets submenus --*/
    menucont = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);
    gtk_widget_show(menucont);
    fMenuList["Presets"] = menucont;

    /* special treatment of preset lists, from gx_preset namespace */
    for (int i = 0; i < GX_NUM_OF_PRESET_LISTS; i++)
      {
	GtkWidget* menuItem =
	  gtk_menu_item_new_with_mnemonic (preset_menu_name[i]);
	gtk_menu_shell_append (GTK_MENU_SHELL(menucont), menuItem);

	GtkWidget* menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuItem), menu);

	gtk_menu_set_accel_path(GTK_MENU(menu), preset_accel_path[i]);

	presmenu[i] = menu;
	presMenu[i] = menuItem;
      }


    /*-- add New Preset saving under Save Presets menu */
    menuitem = gtk_menu_item_new_with_mnemonic ("New _Preset");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_save_newpreset_dialog), NULL);
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_p, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_insert(GTK_MENU_SHELL(presmenu[SAVE_PRESET_LIST]), menuitem, 0);
    gtk_widget_show (menuitem);

    /*-- add a separator line --*/
    GtkWidget* sep = gtk_separator_menu_item_new();
    gtk_menu_shell_insert(GTK_MENU_SHELL(presmenu[SAVE_PRESET_LIST]), sep, 1);
    gtk_widget_show (sep);

    /*-- initial preset list --*/
    gx_preset::gx_build_preset_list();

    vector<string>::iterator it;
    for (it = gx_preset::plist.begin() ; it < gx_preset::plist.end(); it++ )
      {
	const string presname = *it;
	gx_add_preset_to_menus(presname);
      }

    for (int i = 0; i < GX_NUM_OF_PRESET_LISTS; i++)
      gtk_widget_show(presMenu[i]);

    /* ------------------- */

    /*-- add a separator line --*/
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
    gtk_widget_show (sep);

    /*-- Create  Main setting submenu --*/
    menuitem = gtk_menu_item_new_with_mnemonic ("Recall Main _Setting");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_recall_main_setting), NULL);
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_s, GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

    menuitem = gtk_menu_item_new_with_mnemonic ("_Save As Main _Setting");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_save_main_setting), NULL);
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);


    /*-- add a separator line --*/
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
    gtk_widget_show (sep);

    /*-- Create sub menu More Preset Action --*/
    menulabel = gtk_menu_item_new_with_mnemonic("More Preset Options...");
    gtk_menu_shell_append (GTK_MENU_SHELL(menucont), menulabel);
    gtk_widget_show(menulabel);

    menucont = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);
    gtk_widget_show(menucont);
    fMenuList["ExtraPresets"] = menucont;

    /*--------------- Extra preset menu */
    addExtraPresetMenu();
  }


  //------------------------ extra preset menu ----------------------------
  void GxMainInterface::addExtraPresetMenu()
  {
    GtkWidget* menucont;  // menu container
    GtkWidget* menuitem;  // menu item

    menucont = fMenuList["ExtraPresets"];

    /*---------------- Create Presets menu items --------------------*/

    /* forward preset */
    menuitem = gtk_menu_item_new_with_mnemonic("Next _Preset");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_next_preset), NULL);
    gtk_widget_add_accelerator(menuitem, "activate",
			       fAccelGroup, GDK_Page_Down,
			       GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show(menuitem);

    /* rewind preset */
    menuitem = gtk_menu_item_new_with_mnemonic("Previous _Preset");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_previous_preset), NULL);
    gtk_widget_add_accelerator(menuitem, "activate",
			       fAccelGroup, GDK_Page_Up,
			       GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show(menuitem);

    /*-- add a separator line --*/
    GtkWidget* sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
    gtk_widget_show (sep);

    /*-- Create  menu item Delete Active preset --*/
    menuitem = gtk_menu_item_new_with_mnemonic ("_Save _Active Preset");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_save_oldpreset), (gpointer)1);
    gtk_widget_add_accelerator(menuitem, "activate",
			       fAccelGroup, GDK_s,
			       GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

    menuitem = gtk_menu_item_new_with_mnemonic ("_Rename _Active Preset");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_rename_active_preset_dialog), NULL);
    gtk_widget_add_accelerator(menuitem, "activate",
			       fAccelGroup, GDK_r,
			       GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

    menuitem = gtk_menu_item_new_with_mnemonic ("_Delete Active Preset");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_delete_active_preset_dialog), NULL);
    gtk_widget_add_accelerator(menuitem, "activate",
			       fAccelGroup, GDK_Delete,
			       GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

    /*-- add a separator line --*/
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
    gtk_widget_show (sep);

    /*-- Create  menu item Delete All presets --*/
    menuitem = gtk_menu_item_new_with_mnemonic ("_Delete All Presets");
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_delete_all_presets_dialog), NULL);
    gtk_widget_add_accelerator(menuitem, "activate",
			       fAccelGroup, GDK_d,
			       GdkModifierType(GDK_CONTROL_MASK|GDK_SHIFT_MASK), GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

  }

  //----------------------------- option menu ----------------------------
  void GxMainInterface::addOptionMenu()
  {
    GtkWidget* menulabel; // menu label
    GtkWidget* menucont;  // menu container
    GtkWidget* menuitem;  // menu item

    menucont = fMenuList["Top"];

    /*---------------- Create Options menu items ------------------*/
    menulabel = gtk_menu_item_new_with_mnemonic ("_Options");
    gtk_menu_bar_append (GTK_MENU_BAR(menucont), menulabel);
    gtk_widget_show(menulabel);

    /*-- Create Options submenu  --*/
    menucont = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);
    gtk_widget_show(menucont);
    fMenuList["Options"] = menucont;

    /*-- Create oscilloscope check menu item under Options submenu --*/
    menuitem = gtk_check_menu_item_new_with_mnemonic ("_Oscilloscope");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_o, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_show_oscilloscope), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

    /*-- Create tuner check menu item under Options submenu --*/
    menuitem = gtk_check_menu_item_new_with_mnemonic ("_Tuner");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_t, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_tuner), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

    /*-- Create log window check menu item under Options submenu --*/
    menuitem = gtk_check_menu_item_new_with_mnemonic ("Open/Close _Log message");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_l, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_log_window), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM (menuitem), FALSE);
    gtk_widget_show (menuitem);

    /*-- add a separator line --*/
    GtkWidget* sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
    gtk_widget_show (sep);

    /*-- Create Open check menu item under Options submenu --*/
    menuitem = gtk_check_menu_item_new_with_mnemonic ("_Meterbridge");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_m, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_start_stop_meterbridge), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

    /*-- Create Open check menu item under Options submenu --*/
    menuitem = gtk_menu_item_new_with_mnemonic ("_Jack Capture Settings");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_j, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    g_signal_connect(GTK_OBJECT (menuitem), "activate",
		     G_CALLBACK (gx_child_process::gx_show_jack_capture_gui), NULL);
    gtk_widget_show (menuitem);

    /*-- add a separator line --*/
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), sep);
    gtk_widget_show (sep);

    /*-- Create skin menu under Options submenu--*/
    addGuiSkinMenu();
  }


  //----------------------------- skin menu ----------------------------
  void GxMainInterface::addGuiSkinMenu()
  {
    GtkWidget* menulabel; // menu label
    GtkWidget* menucont;  // menu container
    GtkWidget* menuitem;  // menu item
    GSList   * group = NULL;

    menucont = fMenuList["Options"];

    /*---------------- Create skin menu items ------------------*/
    menulabel = gtk_menu_item_new_with_mnemonic ("_Skin...");
    gtk_menu_append (GTK_MENU(menucont), menulabel);
    gtk_widget_show(menulabel);

    menucont = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);
    gtk_widget_show(menucont);
    fMenuList["Skin"] = menucont;

    /* Create black skin item under skin submenu --*/
    guint idx = 0;

    while (idx < skin_list.size())
      {
	menuitem =
	  gtk_radio_menu_item_new_with_label (group, skin_list[idx].c_str());

	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM (menuitem), FALSE);
	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (gx_change_skin), GINT_TO_POINTER(idx));
	gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
	gtk_widget_show (menuitem);
	idx++;
      }

    menucont = fMenuList["Options"];

    menuitem = gtk_menu_item_new();
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_s, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_cycle_through_skin), NULL);
    gtk_menu_append (GTK_MENU(menucont), menuitem);
    gtk_widget_show (menuitem);

    /*-- End skin menu declarations --*/
  }

  //----------------------------- about menu ----------------------------
  void GxMainInterface::addAboutMenu()
  {
    GtkWidget* menulabel; // menu label
    GtkWidget* menucont;  // menu container
    GtkWidget* menuitem;  // menu item

    menucont = fMenuList["Top"];

    /*---------------- Start About menu declarations ----------------*/
    menulabel = gtk_menu_item_new_with_mnemonic ("_About");
    gtk_menu_bar_append (GTK_MENU_BAR(menucont), menulabel);
    gtk_widget_show(menulabel);

    /*-- Create About submenu --*/
    menucont = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);

    /*-- Create About menu item under About submenu --*/
    menuitem = gtk_menu_item_new_with_mnemonic ("_About");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_a, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    g_signal_connect(GTK_OBJECT (menuitem), "activate",
		     G_CALLBACK (gx_show_about), NULL);
    gtk_widget_show (menuitem);

    /*-- Create Help menu item under About submenu --*/
    menuitem = gtk_menu_item_new_with_mnemonic ("_Help");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_h, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    //    g_signal_connect(GTK_OBJECT (menuitem), "activate", G_CALLBACK (gx_show_about), NULL);
    gtk_widget_show (menuitem);

    /*---------------- End About menu declarations ----------------*/
  }

  /*---------------- Jack Server Menu ----------------*/
  void GxMainInterface::addJackServerMenu()
  {
    GtkWidget* menulabel; // menu label
    GtkWidget* menucont;  // menu container
    GtkWidget* menuitem;  // menu item
    GSList   * group = NULL;

    menucont = fMenuList["Engine"];

    /*-- Create Jack Connection toggle button --*/
    menuitem = gtk_check_menu_item_new_with_mnemonic ("Jack Server _Connection ");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_c, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_jack::gx_jack_connection), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);

    gtk_widget_show (menuitem);
    fJackConnectItem = menuitem;

    /*-- create Jack Ports menu item --*/
    menuitem = gtk_check_menu_item_new_with_mnemonic ("Jack _Ports ");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_p, GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_show_portmap_window), NULL);

    g_signal_connect_swapped(G_OBJECT(fPortMapWindow), "delete_event",
			     G_CALLBACK(gx_hide_portmap_window), menuitem);


    gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);

    menucont = fMenuList["Engine"];

    /* add item for client tab cycling through */
    menuitem = gtk_menu_item_new_with_mnemonic("Next Client _Tab");
    gtk_widget_add_accelerator(menuitem, "activate", fAccelGroup,
			       GDK_t, GDK_NO_MOD_MASK, GTK_ACCEL_VISIBLE);
    g_signal_connect (GTK_OBJECT (menuitem), "activate",
		      G_CALLBACK (gx_cycle_through_client_tabs), NULL);
    gtk_menu_append (GTK_MENU_SHELL(menucont), menuitem);
    gtk_widget_show (menuitem);


    /*-- Create  Latency submenu under Jack Server submenu --*/
    menulabel = gtk_menu_item_new_with_mnemonic ("_Latency");
    gtk_menu_append (GTK_MENU(menucont), menulabel);
    gtk_widget_show(menulabel);

    menucont = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menulabel), menucont);

    /*-- Create  menu item under Latency submenu --*/
    gchar buf_size[8];
    const int min_pow = 5;  // 2**5  = 32
    const int max_pow = 13; // 2**13 = 8192
    group = NULL;

    for (int i = min_pow; i <= max_pow; i++)
      {
	int jack_buffer_size = (int)pow(2.,i);
	(void)snprintf(buf_size, 5, "%d", jack_buffer_size);
	menuitem = gtk_radio_menu_item_new_with_label (group, buf_size);
	group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (menuitem));
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem), FALSE);

	g_signal_connect (GTK_OBJECT (menuitem), "activate",
			  G_CALLBACK (gx_jack::gx_set_jack_buffer_size),
			  GINT_TO_POINTER(jack_buffer_size));

	// display actual buffer size as default
	gtk_menu_shell_append(GTK_MENU_SHELL(menucont), menuitem);
	gtk_widget_show (menuitem);

	fJackLatencyItem[i-min_pow] = menuitem;
      }
  }

  /* -------- init jack client menus ---------- */
  void GxMainInterface::initClientPortMaps()
  {
    // make sure everything is reset
    deleteAllClientPortMaps();

    // if jack down, no bother
    // (should not be called when jack is down anyway)
    if (!gx_jack::client)
      return;

    // get all existing port names (no MIDI stuff for now)
    const char** iportnames =
      jack_get_ports(gx_jack::client, NULL,
		     JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput);

    const char** oportnames =
      jack_get_ports(gx_jack::client, NULL,
		     JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput);

    // populating output port menus
    int p = 0;
    while (oportnames[p] != 0)
      {
	const string pname = oportnames[p];
	if (pname.substr(0, pname.find(":")) != gx_jack::client_name)
	  gx_client_port_queue[pname] = JackPortIsOutput;

	p++;
      }

    // populating input port menus
    p = 0;
    while (iportnames[p] != 0)
      {
	const string pname = iportnames[p];
	if (pname.substr(0, pname.find(":")) != gx_jack::client_name)
	  gx_client_port_queue[pname] = JackPortIsInput;
	p++;
      }

    // free port name lists (cf. JACK API doc)
    free(iportnames);
    free(oportnames);
  }

  /* -------- add  jack client item ---------- */
  void GxMainInterface::addClientPortMap(const string clname)
  {
    // no need to bother if are not a jack client
    if (gx_jack::client == NULL)
      {
	gx_print_warning("Jack Client", "Connect back to jack first");
	return;
      }

    // we don't want these guys here :)
    if (clname == gx_jack::client_name ||
	clname == "ardourprobe"        ||
	clname == "freewheel"          ||
	clname == "qjackctl"           ||
	clname == "Patchage")
      return;

    // add tab in client notebook if needed
    // Note: one-to-one mapping: only ONE tab per client
    if (getClientPortMap(clname))
      return;

    GtkWidget* label   = gtk_label_new(clname.c_str());

    GtkWidget* mapbox  = gtk_hbox_new(TRUE, 10);
    gtk_widget_set_name(mapbox, clname.c_str());

    for (int t = gx_jack::kAudioInput; t <= gx_jack::kAudioOutput2; t++)
      {
	GtkWidget* table  = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(mapbox), table, TRUE, FALSE, 0);

    g_signal_connect(table, "expose-event", G_CALLBACK(box6_expose), NULL);
	gtk_widget_show(table);
      }

    gtk_notebook_append_page(fPortMapTabs, mapbox, label);
    gtk_widget_show(label);
    gtk_widget_show(mapbox);
    fClientPortMap.insert(mapbox);

    // move focus back to guitarix main window
    gtk_widget_grab_focus(fWindow);

    gx_print_info("Jack Client",  clname + " added to port map");
  }

  /* -------- add port to a given jack client portmap  ---------- */
  void GxMainInterface::addClientPorts()
  {

    // no need to bother if are not a jack client
    if (gx_jack::client == NULL)
      {
	gx_print_warning("Jack Client Port Add",
			 "we are not yet a jack client");
	gx_client_port_queue.clear();
	return;
      }

    // go through list
    map<string, int>::iterator pn;
    for (pn  = gx_client_port_queue.begin();
	 pn != gx_client_port_queue.end();
	 pn++)
      {
	string port_name = pn->first;

	// retrieve the client name from the port name
	string client_name = port_name.substr(0, port_name.find(':'));
	string short_name  = port_name.substr(port_name.find(':')+1);

	// if client portmap does not exist, create it
	if (!getClientPortMap(client_name))
	  addClientPortMap(client_name);

	if (!getClientPortMap(client_name))
	  continue;

	// port flags
	int flags          = pn->second;

	// set up how many port tables we should deal with:
	// 1 for guitarix input (mono)
	// 2 for guitarix outputs (stereo)

	int table_index = gx_jack::kAudioInput, ntables = 1;
	if ((flags & JackPortIsOutput) == 0)
	  {
	    table_index = gx_jack::kAudioOutput1;
	    ntables = 2;
	  }

	// add port item
	for (int i = table_index; i < table_index + ntables; i++)
	  {
	    // retrieve port table
	    GtkVBox* portbox =
	      GTK_VBOX(getClientPortTable(client_name, i));
        gtk_container_set_border_width (GTK_CONTAINER (portbox), 8);
	    // create checkbutton
	    GtkWidget* button =
	      gtk_check_button_new_with_label(short_name.c_str());
        GtkWidget *button_text = gtk_bin_get_child(GTK_BIN(button));


        GdkColor colorGreen;
        GdkColor color1;
        GdkColor color2;
        gdk_color_parse("#000000", &colorGreen);
        gtk_widget_modify_fg (button_text, GTK_STATE_NORMAL, &colorGreen);
        gdk_color_parse("#292995", &color1);
        gtk_widget_modify_fg (button_text, GTK_STATE_ACTIVE, &color1);
        gdk_color_parse("#444444", &color2);
        gtk_widget_modify_fg (button_text, GTK_STATE_PRELIGHT, &color2);
        GtkStyle *style = gtk_widget_get_style(button_text);
        pango_font_description_set_size(style->font_desc, 8*PANGO_SCALE);
        pango_font_description_set_weight(style->font_desc, PANGO_WEIGHT_BOLD);
        gtk_widget_modify_font(button_text, style->font_desc);

	    gtk_widget_set_name(button,  (gchar*)port_name.c_str());
	    gtk_box_pack_start(GTK_BOX(portbox), button, FALSE, FALSE, 0);
	    g_signal_connect(GTK_OBJECT (button), "toggled",
			     G_CALLBACK (gx_jack::gx_jack_port_connect),
			     GINT_TO_POINTER(i));
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
	    gtk_widget_show_all(button);
	  }
      }

    // empty queue
    gx_client_port_queue.clear();

    // move focus back to guitarix main window
    gtk_widget_grab_focus(fWindow);
  }

  /* -------- delete port lists for a given jack client ---------- */
  void GxMainInterface::deleteClientPorts()

  {
    // no need to bother if are not a jack client
    if (gx_jack::client == NULL)
      {
	gx_print_warning("Jack Client Port Add",
			 "we are not yet a jack client");
	gx_client_port_dequeue.clear();
	return;
      }

    if (gx_client_port_dequeue.empty())
      return;

    // go through list
    string clname;

    map<string, int>::iterator pn;
    for (pn = gx_client_port_dequeue.begin(); pn != gx_client_port_dequeue.end(); pn++)
      {
	string port_name = pn->first;

	// delete port item to be displayed as a submenu if some ports exist
	clname = port_name.substr(0, port_name.find(':'));

	// check that portmap does exists, otherwise, no point
	if (!getClientPortMap(clname))
	  break;

	// lookup port tables
	for (int l = 0; l < NUM_PORT_LISTS; l++)
	  {
	    GtkWidget* wd = getClientPort(port_name, l);
	    if (wd)
	      gtk_widget_destroy(wd);
	  }
      }

    // we could delete the tab if needed
    bool mapempty = true;
    if (getClientPortMap(clname))
      {
	for (int l = 0; l < NUM_PORT_LISTS; l++)
	  {
	    GtkWidget* wd = getClientPortMap(clname);
	    if (wd)
	      {
		GList* list =
		  gtk_container_get_children(GTK_CONTAINER(wd));

		if (g_list_length(list) > 0)
		  {
		    mapempty = false;
		    break;
		  }
	      }
	  }
      }

    if (mapempty)
      deleteClientPortMap(clname);

    // empty queue
    gx_client_port_dequeue.clear();

    // move focus back to guitarix main window
    gtk_widget_grab_focus(fWindow);


  }

  /* -------- delete jack client item ---------- */
  void GxMainInterface::deleteClientPortMap(string clname)
  {
    // no need to delete it if nothing to delete
    GtkWidget* tab = getClientPortMap(clname);
    if (!tab)
      return;

    // remove it from our list
    fClientPortMap.erase(fClientPortMap.find(tab));

    // remove the notebook tab
    int page = gtk_notebook_page_num(fPortMapTabs, tab);
    gtk_notebook_remove_page(fPortMapTabs, page);

    // destroy the widget
    if (GTK_IS_WIDGET(tab))
      gtk_widget_destroy(tab);

    // move focus back to guitarix main window
    gtk_widget_grab_focus(fWindow);

    // print warning
    gx_print_warning("Jack Client Delete", clname + " portmap deleted");
  }

  /* -------- delete all jack client menus ---------- */
  void GxMainInterface::deleteAllClientPortMaps()
  {
    // don't do it if nothing to do
    if (fClientPortMap.empty())
      return;

    set<GtkWidget*>::iterator it;

    // all port maps deletion
    for (it = fClientPortMap.begin(); it != fClientPortMap.end(); it++)
      {
	GtkWidget* mapbox = *it;

	int page = gtk_notebook_page_num(fPortMapTabs, mapbox);
	gtk_notebook_remove_page(fPortMapTabs, page);

	if (GTK_IS_WIDGET(mapbox))
	  gtk_widget_destroy(mapbox);
      }

    fClientPortMap.clear();

    // move focus back to guitarix main window
    gtk_widget_grab_focus(fWindow);

    // print warning
    gx_print_warning("Jack Client Delete All",
		     "All client portmaps have been deleted");
  }

  /* ---------------- retrieve a client port widget --------------- */
  GtkWidget* GxMainInterface::getClientPort(const string port_name,
					    const int    tab_index)
  {

    // client name
    string clname = port_name.substr(0, port_name.find(':'));

    // get client port table
    GtkWidget* table = getClientPortTable(clname, tab_index);
    if (!table)
      return NULL;

    // get list of elements
    GList* list = gtk_container_get_children(GTK_CONTAINER(table));

    // retrieve element
    for (guint p = 0; p < g_list_length(list); p++)
      {
	GtkWidget* wd = (GtkWidget*)g_list_nth_data(list, p);
	if (port_name == gtk_widget_get_name(wd))
	  return wd;
      }

    return NULL;
  }

  /* --------------- retrieve a client port table widget -------------- */
  GtkWidget* GxMainInterface::getClientPortTable(const string clname,
						 const int    index)
  {

    // get port map
    GtkWidget* portmap = getClientPortMap(clname);
    if (!portmap)
      return NULL;

    // look up list of vboxes in portmap
    GList* list = gtk_container_get_children(GTK_CONTAINER(portmap));
    return (GtkWidget*)g_list_nth_data(list, index);
  }

  /* ----------------- retrieve a client portmap widget --------------- */
  GtkWidget* GxMainInterface::getClientPortMap(const string clname)
  {
    // try to find a match
    set<GtkWidget*>::iterator it;

    for (it = fClientPortMap.begin(); it != fClientPortMap.end(); it++)
      if (clname == gtk_widget_get_name(*it))
	return *it; // got it

    return NULL;
  }

  /* -------- user interface builder ---------- */
  void GxMainInterface::setup()
  {
    //----- notebook window with tabs representing jack clients and portmaps
    // Note: out of box stack scheme.
    createPortMapWindow("Jack Port Maps");

    gx_engine::GxEngine* engine = gx_engine::GxEngine::instance();

    //----- the main box, all visible widgets are a child of this box
    openVerticalBox("");

    //----- add the menubar on top
    {
      addMainMenu();

      //----- this is a dummy widget, only for save settings for the latency warning dialog
      openWarningBox("WARNING", &engine->fwarn);
      setSkinBox("SKIN", &engine->fskin);
      closeBox();

      //----- the upper box,
      openVerticalBox("");
      {
	openEventBox(" ");
	openPaintBox2("");
	{
	  openHorizontalBox("");
	  {
	    //----- the tuner widget
	    openVerticalBox("");
	    {
	      addNumDisplay("", &engine->fConsta1);
	    }
	    closeBox();

	    //----- the balance widget
	    openVerticalBox("");
	    {
	      addslider("balance", &engine->fslider25, 0.f, -1.f, 1.f, 1.e-01f);
	    }
	    closeBox();

	    //----- the jconv widget on the main window
	    openFrameBox("");
	    {
	      openExpanderBox(" jconv ", &engine->fexpand2);
	      {
		openHandleBox("  ");
		{
		  openVerticalBox("");
		  {
		    openHorizontalBox("");
		    {
		      addregler("gain", &engine->fjc_ingain,  0.f, -20.f, 20.f, 0.1f);
		      addslider("wet/dry", &engine->fslider24,  0.f, -1.f, 1.f, 1.e-01f);
		    }
		    closeBox();
		    addJConvButton("jconv settings", &engine->filebutton);
		    addJToggleButton("run jconv", &gx_jconv::GxJConvSettings::checkbutton7);
		  }
		  closeBox();

		}
		closeBox();

	      }
	      closeBox();

	    }
	    closeBox();

	  }
	  closeBox();
	}
	closeBox();
      }
      closeBox();
      //----- end of the upper box

      //----- the middle box,
      openExpanderBox(" CONTROLS ", &engine->fexpand);
      {


	//----- a handle box is a vertical box
	openHandleBox("  ");
	{
	  //----- arange all widgets in a horizontal scale
	  openHorizontalBox("");
	  {

	    //----- arange all widgets in a vertical scale
	    openVerticalBox("");
	    {
	      // openPaintBox("");
	      // {
	      //----- the left widget side, volume, tone, fuzz
	      openHorizontalBox("");
	      {

		//----- open a box for the volume controllers
		{
            openFrameBox("");
		    closeBox();

		  openVerticalBox("");
		  {
		    openFrameBox("");
		    closeBox();

		    // add a signal level box: out of box stack, no need to closeBox
		    openSignalLevelBox("Signal Level");

		  }
		  closeBox();

		  openVerticalBox("volume");
		  {

		    openVerticalBox("");
		    {
		      addbigregler(" in / level ", &engine->fslider3, 0.f, -40.f, 40.f, 0.1f);
		      addbigregler("out / master", &engine->fslider17, 0.f, -40.f, 40.f, 0.1f);
		    }
		    closeBox();

		  }
		  closeBox();
		  //----- volume controll ready

		  //----- open a box for the tone and the fuzz controllers
		  openVerticalBox("tone");
		  {
		    addregler("bass",   &engine->fslider_tone2, 0.f, -20.f, 20.f, 0.1f);
		    addregler("middle", &engine->fslider_tone1, 0.f, -20.f, 20.f, 0.1f);
		    addregler("treble", &engine->fslider_tone0, 0.f, -20.f, 20.f, 0.1f);

		    //----- I have set a framebox to fill space beetween the widgets
		    openFrameBox("");
		    closeBox();
		  }
		  closeBox();

		}
		closeBox();
		//----- end tone and fuzz controll

		//----- the next part in the vertical box,
		openVerticalBox("");
		{
		  openHorizontalBox("");
		  {

		    openVerticalBox("shaper ");
		    {
		      addswitch("", &engine->fng);
		      openFrameBox("");
		      {
			addHorizontalWheel("sharper",&engine->fsharp0, 1.f, 1.f, 10.f, 1.0f);
		      }
		      closeBox();
		      openFrameBox("");
		      {
		      }
		      closeBox();
		    }
		    closeBox();
		    openVerticalBox("noise gate ");
		    {
		      addswitch("", &engine->fnoise_g);
		      addHorizontalWheel(" threshold ", &engine->fnglevel, 0.017f, 0.01f, 0.21f, 0.001f);
		      openFrameBox("");
		      {
		      }
		      closeBox();
		    }
		    closeBox();

		    openVerticalBox("anti aliase");
		    {
		      addswitch("", &engine->antialis0);
		      addHorizontalWheel(" feedback ", &engine->faas1, 0.3f, 0.3f, 0.9f, 0.01f);
		      openFrameBox("");
		      {
		      }
		      closeBox();
		    }
		    closeBox();
		  }
		  closeBox();

		  // openPaintBox("");
		  // {
		  openVerticalBox("amp  ");
		  {
		    openHorizontalBox("");
		    {
		      openFrameBox("");
		      {
		      }

		      closeBox();
		      addminiswitch(" oversample  ", &engine->fupsample);
		      openFrameBox("");
		      {
		      }

		      closeBox();
		      addminiswitch(" bass booster", &engine->fboost);
		      openFrameBox("");
		      {
		      }
		      closeBox();
		    }
		    closeBox();
		  }
		  closeBox();

		  //-----

		  //----- the second part in the vertical box
		  openHorizontalBox("");
		  {
		    openVerticalBox("");
		    {
		      openVerticalBox("");
		      {
			addswitch("preamp", &engine->fcheckbox1);
			addHorizontalWheel("atan",&engine->fatan, 1.f, 1.f, 10.f, 1.0f);
		      }
		      closeBox();
		      openFrameBox("");
		      {
		      }
		      closeBox();
		      openVerticalBox("");
		      {
			addswitch("drive", &engine->fprdr);
			addHorizontalWheel("drive", &engine->fpredrive, 1.f, 1.f, 10.f, 1.0f);
		      }
		      closeBox();

		    }
		    closeBox();
		    openFrameBox("");
		    closeBox();

		    openVerticalBox("");
		    {
		      openVerticalBox("");
		      {
			addswitch("tube", &engine->ftube);
			addHorizontalWheel("tube",&engine->ffuzzytube, 1.f, -3.f, 10.f, 1.0f);
		      }
		      closeBox();
		      openFrameBox("");
		      {
		      }
		      closeBox();
		      openVerticalBox("");
		      {
			addswitch("vibrato", &engine->fresoon);
			addHorizontalWheel("vibrato", &engine->fvibrato, 0.f, 0.f, 2.f, 0.02f);
		      }
		      closeBox();

		    }
		    closeBox();
		    openFrameBox("");
		    closeBox();

		    openVerticalBox("");
		    {
		      openVerticalBox("");
		      {
			addswitch("tube2", &engine->ftube3);
			openFrameBox("");
			{
			  addHorizontalWheel("tube", &engine->fresotube3, 1.f, -3.f, 10.f, 1.0f);
			}
			closeBox();

		      }
		      closeBox();

		      openVerticalBox("");
		      {
			openVerticalBox1("resonanz");
			{
			  addHorizontalWheel("reso", &engine->fresotube1, 0.5f, 0.f, 0.9f, 0.01f);
			}
			closeBox();

			openVerticalBox1("vibrato");
			{
			  addHorizontalWheel("vibrato", &engine->fresotube2, 1.f, 0.f, 1.f, 0.01f);
			}
			closeBox();

		      }
		      closeBox();

		    }
		    closeBox();

		  }
		  closeBox();
		  openFrameBox("");
		  closeBox();
		  openFrameBox("");
		  closeBox();
		  // }
		  // closeBox();
		}
		closeBox();
	      }
	      closeBox();

	      //  }
	      //  closeBox();
	      //----- end
	      openFrameBox("");
	      closeBox();

	      //----- this box include all effects and the osccilloscope


	      openVerticalBox1("");
	      {
		//  openScrollBox("  ");
		//  {

		//----- fill empty space
		// openPaintBox("");
		// {
		openHorizontalBox("");
		{
		}
		closeBox();

		//----- this box include only the effects

		openHorizontalBox("");
		{
		  //  openFrameBox("");
		  //  closeBox();
		  //----- the compressor
		  openVerticalBox("compressor");
		  {
		    addregler("ratio", &engine->fentrycom2, 2.000000f, 1.000000f, 20.000000f, 0.100000f);
		    openHorizontalBox("");
		    {
		      addtoggle("", &engine->fcheckboxcom1);

		      //----- open a dialogbox(toplevel widget) and put the advanced controlls in it
		      openDialogBox("compressor", &engine->fdialogbox8);
		      {
			openHandleBox("  ");
			{
			  openVerticalBox("compressor");
			  {
			    openHorizontalBox("");
			    {
			      addregler("knee",      &engine->fentrycom1, 3.000000f, 0.000000f, 20.000000f, 0.100000f);
			      addregler("ratio",     &engine->fentrycom2, 2.000000f, 1.000000f, 20.000000f, 0.100000f);
			      addregler("threshold", &engine->fentrycom0, -20.000000f, -96.000000f, 10.000000f, 0.100000f);
			    }
			    closeBox();

			    openVerticalBox("envelop");
			    {
			      addslider("attack",  &engine->fslidercom0, 2.000000e-03f, 0.000000f, 1.000000f, 1.000000e-03f);
			      addslider("release", &engine->fslidercom1, 0.500000f, 0.000000f, 10.000000f, 1.000000e-02f);
			    }
			    closeBox();

			  }
			  closeBox();

			}
			closeBox();

		      }
		      closeBox();

		    }
		    //----- end advanced settings widget
		    closeBox();

		  }
		  closeBox();
		  //----- end compressor


		  //----- overdrive
		  openVerticalBox("overdrive");
		  {
		    addregler("  drive ", &engine->drive, 1.f, 1.f, 20.f, 0.1f);
		    addtoggle("", &engine->foverdrive4);
		  }
		  closeBox();
		  //-----end overdrive


		  //----- distortion
		  openVerticalBox(" distortion");
		  {
		    addregler("  drive ", &engine->fslider9, 0.64f, 0.f, 1.f, 1.e-02f);
		    openHorizontalBox("");
		    {
		      addtoggle("", &engine->fcheckbox4);

		      //----- open a dialogbox(toplevel widget) and put the advanced controlls in it
		      {
			openDialogBox("distortion", &engine->fdialogbox1);
			{
			  openHandleBox("  ");
			  {
			    addbigregler("  drive ", &engine->fslider9, 0.64f, 0.f, 1.f, 1.e-02f);
			    addregler("level", &engine->fslider8, 1.000000e-02f, 0.0f, 0.50f, 1.000000e-02f);
			    addregler("gain",  &engine->fslider10, 2.0f, -10.0f, 10.0f, 0.1f);

			    openVerticalBox("low/highpass");
			    {
			      openHorizontalBox("");
			      {
				addregler("high-freq ", &engine->fentry1, 130.0f, 20.0f, 7040.0f, 10.0f);
				addregler(" low-freq ", &engine->fentry0, 5000.0f, 20.0f, 12000.0f, 10.0f);
			      }
			      closeBox();

			      addtoggle("", &engine->fcheckbox2);
			    }
			    closeBox();

			    openVerticalBox("low/highcutoff");
			    {
			      openHorizontalBox("");
			      {
				addregler("high-freq ", &engine->fslider6, 5000.0f, 1000.0f, 12000.0f, 10.0f);
				addregler(" low-freq ", &engine->fslider7, 130.0f, 20.0f, 1000.0f, 10.0f);
			      }
			      closeBox();

			      addtoggle("", &engine->fcheckbox3);
			    }
			    closeBox();

			    openHorizontalBox("resonanz");
			    {
			      addregler("trigger ",  &engine->fslider4, 0.12f, 0.0f, 1.0f, 1.000000e-02f);
			      addregler(" vibrato ", &engine->fslider5, 1.0f, 0.0f, 1.0f, 1.000000e-02f);
			    }
			    closeBox();

			  }
			  closeBox();

			}
			closeBox();

		      }
		      //----- end advanced settings widget
		      closeBox();

		    }
		    closeBox();
		    //----- end distortion

		    //----- freeverb
		    openVerticalBox(" freeverb");
		    {
		      addregler("RoomSize", &engine->fslider16, 0.500000f, 0.000000f, 1.000000f, 2.500000e-02f);

		      openHorizontalBox("");
		      {
			addtoggle("", &engine->fcheckbox6);

			//----- open a dialogbox(toplevel widget) and put the advanced controlls in it
			openDialogBox("freeverb", &engine->fdialogbox2);
			{
			  openHandleBox("  ");
			  {
			    addregler("RoomSize", &engine->fslider16, 0.500000f, 0.000000f, 1.000000f, 2.500000e-02f);
			    addregler("damp", &engine->fslider15, 0.5f, 0.0f, 1.0f, 2.500000e-02f);
			    addregler("wet/dry", &engine->fslider14, 0.0f, -0.5f, 0.5f, 1.e-01f);
			  }
			  closeBox();
			}
			closeBox();

		      }
		      //----- end advanced settings widget
		      closeBox();

		    }
		    closeBox();
		    //----- end freeverb

		    //----- IR
		    openVerticalBox("IR");
		    {
		      openHorizontalBox("");
		      {
			addregler("   freq   ", &engine->fslider21, 440.000000f, 20.000000f, 12000.000000f, 10.000000f);
			//addregler(" peak ", &engine->fslider22, 1.000000f, 0.000000f, 10.000000f, 0.200000f);
		      }
		      closeBox();

		      openHorizontalBox("");
		      {
			addtoggle("", &engine->fcheckbox8);

			//----- open a dialogbox(toplevel widget) and put the advanced controlls in it
			openDialogBox("ImpulseResponse", &engine->fdialogbox3);
			{
			  openHandleBox("  ");
			  {
			    addregler("    freq     ", &engine->fslider21, 440.000000f, 20.000000f, 12000.000000f, 10.000000f);
			    addregler("     peak    ", &engine->fslider22, 1.000000f, 0.000000f, 10.000000f, 0.200000f);
			    addregler("    bandwidth    ", &engine->fslider20, 100.0f, 20.0f, 20000.0f, 10.0f);
			  }
			  closeBox();

			}
			closeBox();

		      }
		      //----- end advanced settings widget
		      closeBox();

		    }
		    closeBox();
		    //----- end IR

		    openVerticalBox("chorus");
		    {

		      addregler("level", &engine->fslider_CH3, 0.5f, 0.0f, 1.0f, 1.000000e-02f);
		      openHorizontalBox("");
		      {
			addtoggle("", &engine->fchorus);
			openDialogBox("chorus", &engine->fchorusbox);
			{
			  openHandleBox("  ");
			  {
			    openVerticalBox("");
			    {
			      openHorizontalBox("");
			      {
				addregler("  delay  ", &engine->fslider_CH2, 2.500000e-02f, 0.0f, 0.2f, 1.000000e-03f);
				addregler("  depth  ", &engine->fslider_CH1, 2.000000e-02f, 0.0f, 1.0f, 1.000000e-03f);
				addregler("  freq  ", &engine->fslider_CH0, 3.0f, 0.0f, 10.0f, 1.000000e-02f);
				addregler("  level  ", &engine->fslider_CH3, 0.5f, 0.0f, 1.0f, 1.000000e-02f);

			      }
			      closeBox();
			    }
			    closeBox();
			  }
			  closeBox();
			}
			closeBox();
		      }
		      closeBox();
		    }
		    closeBox();

		    //----- crybaby
		    openVerticalBox("crybaby");
		    {
		      addregler(" wah ", &engine->fslider11, 0.000000f, 0.000000f, 1.000000f, 1.000000e-02f);

		      openHorizontalBox("");
		      {
			addtoggle("", &engine->fcheckbox5);

			//----- open a dialogbox(toplevel widget) and put the advanced controlls in it
			openDialogBox("crybaby", &engine->fdialogbox4);
			{
			  openHandleBox("  ");
			  {
			    openVerticalBox("");
			    {
			      openHorizontalBox("");
			      {
				addregler("  wah   ", &engine->fslider11, 0.0f, 0.0f, 1.0f, 1.000000e-02f);
				addregler("  level  ", &engine->fslider12, 0.1f, 0.0f, 1.0f, 1.000000e-02f);
				addregler("wet/dry", &engine->fslider13, 0.f, -1.f, 1.f, 1.e-01f);
			      }
			      closeBox();

			      addminiswitch(" autowah", &engine->fautowah);
			    }
			    closeBox();
			  }
			  closeBox();

			}
			closeBox();

		      }
		      //----- end advanced settings widget
		      closeBox();

		    }
		    closeBox();
		    //----- end crybaby

		    //----- echo
		    openVerticalBox("echo");
		    {
		      openHorizontalBox("");
		      {
			addregler("    %    ", &engine->fslider19, 0.000000f, 0.000000f, 100.000000f, 0.100000f);
			addregler("  time  ", &engine->fslider18, 1.000000f, 1.000000f, 2000.000000f, 1.000000f);
		      }
		      closeBox();

		      addtoggle("", &engine->fcheckbox7);

		    }
		    closeBox();

		  }
		  //----- end echo

		}
		closeBox();
		// }
		// closeBox();
		//----- close vertical effect box


		//----- open a box for the oscilloscope
		openVerticalBox1("");
		{


		  openHorizontalBox("");
		  {


		    //----- the oscilloscope
		    addLiveWaveDisplay(" ", &engine->viv , &engine->vivi);

		  }
		  closeBox();
		  // openPaintBox("");
		  // {

		  openHorizontalBox("");
		  {

		    openVerticalBox("");
		    {
		      openFrameBox("");
		      closeBox();
		      openFrameBox("");
		      closeBox();
		      addregler("fuzz",&engine->fthreshold, 1.f, 0.f, 1.f, 0.01f);
		      addHorizontalSlider("threshold", &engine->ffuse, 0.f, 0.f, 2.f, 1.f);
		    }
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();

		    openFrameBox("");
		    closeBox();
		  }
		  closeBox();



		  //----- fill empty space
		  openFrameBox("");
		  closeBox();

		  //  }
		  //  closeBox();
		  //}
		  //closeBox();

		}
		//----- close ocsilloscope box
		closeBox();
	      }
	      closeBox();
	    }
	    closeBox();

	    openFrameBox("");
	    closeBox();


	  }
	  closeBox();


	}
	closeBox();
	//----- close the vertical box for effects and oscilloscope

	//----- open the 3. box on botton of the main widget
	openEventBox(" FEEDBACK ");
	{
	  openHorizontalBox("");
	  {
	    openVerticalBox("");
	    {
	      //----- the midi widget
	      openVerticalMidiBox("");
	      {
		openHorizontalBox("midi_out");
		{
		  //----- create the midi settings dialog
		  openDialogBox("midi out", &engine->fdialogbox6);
		  {
		    openTabBox("");
		    {
		      openVerticalBox("channel1");
		      {
			openEventBox(" ");
			{
			  openHorizontalBox("");
			  {
			    addregler("velocity", &engine->fslider26, 64.f, 0.f, 127.f, 1.f);

			    openVerticalBox("");
			    {
			      addregler("volume", &engine->fslider46, 64.f, 0.f, 127.f, 1.f);
			      addCheckButton("autogain", &engine->fautogain);
			    }
			    closeBox();

			    openVerticalBox("");
			    {
			      addNumEntry("channel 1", &engine->fslider30, 0.f, 0.f, 16.f, 1.f);
			      addNumEntry("program", &engine->fslider31, 0.f, 0.f, 248.f, 1.f);
			    }
			    closeBox();

			    addregler("oktave", &engine->fslider29, 0.f, -2.f, 2.f, 1.f);
			    addregler("sensity", &engine->fslider27, 20.f, 1.f, 500.f, 1.f);
			  }
			  closeBox();

			}
			closeBox();

			openHorizontalBox("");
			{
			  openHorizontalBox(" ");
			  closeBox();


			  openHorizontalBox(" ");
			  {
			    addPToggleButton("auto pitch", &engine->fpitch);
			  }
			  closeBox();

			}
			closeBox();

		      }
		      closeBox();


		      openVerticalBox("channel2");
		      {

			openEventBox(" ");
			{

			  openHorizontalBox("");
			  {
			    addregler("velocity", &engine->fslider32, 64.f, 0.f, 127.f, 1.f);

			    openVerticalBox("");
			    {
			      addregler("volume", &engine->fslider47, 64.f, 0.f, 127.f, 1.f);
			      addCheckButton("autogain", &engine->fautogain1);
			    }
			    closeBox();

			    openVerticalBox("");
			    {
			      addNumEntry("channel 2", &engine->fslider35, 0.f, 0.f, 16.f, 1.f);
			      addNumEntry("program", &engine->fslider36, 0.f, 0.f, 248.f, 1.f);
			    }
			    closeBox();

			    addregler("oktave", &engine->fslider34, 0.f, -2.f, 2.f, 1.f);
			    addregler("sensity", &engine->fslider33, 20.f, 1.f, 500.f, 1.f);

			  }
			  closeBox();

			}
			closeBox();

			openHorizontalBox("");
			{
			  addtoggle("", &engine->fcheckbox10);

			  openHorizontalBox(" ");
			  {
			    addPToggleButton("auto pitch", &engine->fpitch1);
			  }
			  closeBox();

			}
			closeBox();

		      }
		      closeBox();

		      openVerticalBox("channel3");
		      {
			openEventBox(" ");

			openHorizontalBox("");
			{
			  addregler("velocity", &engine->fslider40, 64.f, 0.f, 127.f, 1.f);

			  openVerticalBox("");
			  {
			    addregler("volume", &engine->fslider48, 64.f, 0.f, 127.f, 1.f);
			    addCheckButton("autogain", &engine->fautogain2);
			  }
			  closeBox();

			  openVerticalBox("");
			  {
			    addNumEntry("channel 3", &engine->fslider44, 0.f, 0.f, 16.f, 1.f);
			    addNumEntry("program", &engine->fslider43, 0.f, 0.f, 248.f, 1.f);
			  }
			  closeBox();

			  addregler("oktave", &engine->fslider42, 0.f, -2.f, 2.f, 1.f);
			  addregler("sensity", &engine->fslider41, 20.f, 1.f, 500.f, 1.f);

			}
			closeBox();

		      }
		      closeBox();

		      openHorizontalBox("");
		      {
			addtoggle("", &engine->fcheckbox11);

			openHorizontalBox(" ");
			{
			  addPToggleButton("auto pitch", &engine->fpitch2);
			}
			closeBox();

		      }
		      closeBox();

		    }
		    closeBox();

		    openVerticalBox("beat_detector");
		    {
		      openEventBox(" ");
		      {
			openHorizontalBox("");
			{
			  addregler("stepper", &engine->fslider39, 1.f, 1.f, 32.f, 1.f);
			  addregler("note_off", &engine->fslider37, 2.f, 1.f, 127.f, 1.f);
			  addregler("atack_gain", &engine->fslider45, 5.f, 1.f, 127.f, 1.f);
			  addregler("beat_gain", &engine->fslider38, 1.f, 0.0f, 127.f, 1.f);
			}
		      }
		      closeBox();

		    }
		    closeBox();

		    addStatusDisplay("", &engine->midistat);

		    openVerticalBox("Midi gain");
		    {
		      addHorizontalWheel("midi gain", &engine->midi_gain, 0.f, -20.f, 60.f, 1.f);
		    }
		    closeBox();

		  }
		  closeBox();

		}
		closeBox();

	      }
	      closeBox();

	    }
	    closeBox();

	  }
	  closeBox();

	}
	closeBox();


	//----- add the controlers for feedback feedforward to the bottob box
	addslider(" feedback", &engine->fslider0, 0.000000f, -1.000000f, 1.000000f, 1.000000e-02f);
	addslider(" feedforward", &engine->fslider23, 0.000000f, -1.000000f, 1.000000f, 1.000000e-02f);

	//----- the toggle button to start/stop jack_capture
	openFrameBox("");
	{
	  addToggleButton("record", 0);
	}
	closeBox();
	//----- end jack_capture

      }
      closeBox();
      //----- close botton box
    }

    closeBox();
    //----- close main box

    // add a log message box: out of box stack, no need to closeBox
    openTextLoggingBox("Logging Window");


    closeBox();
  }


  //---- show main GUI
  void GxMainInterface::show()
  {
    assert(fTop == 0);

    fInitialized = true;

    if (gx_jack::client)
      {
	// refresh some GUI stuff
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(fJackConnectItem), TRUE);

	GtkWidget* wd = getJackLatencyItem(gx_jack::jack_bs);
	if (wd) gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(wd), TRUE);

	gtk_window_set_title(GTK_WINDOW(fWindow), gx_jack::client_name.c_str());

	// build port menus for existing jack clients
	initClientPortMaps();

      }
    else
      {
	gtk_widget_hide(gx_gui::gx_jackd_on_image);
	gtk_widget_show(gx_gui::gx_jackd_off_image);
      }

    gtk_widget_show  (fBox[0]);
    gtk_widget_show  (fWindow);
  }

  //---- show main GUI thread and more
  void GxMainInterface::run()
  {
    string previous_state = gx_user_dir + gx_jack::client_name + "rc";
    recallState(previous_state.c_str());

    //----- set the state for the latency change warning widget
    gx_engine::GxEngine::instance()->set_latency_warning_change();

    //----- set the last used skin when no cmd is given
    if (no_opt_skin == 1)
      gx_set_skin_change(gx_engine::GxEngine::instance()->fskin);

    g_timeout_add(40, gx_update_all_gui, 0);

    /* timeout in milliseconds */
    g_timeout_add(20,  gx_refresh_signal_level, 0);
    g_timeout_add(20,  gx_refresh_jcsignal_level, 0);
    g_timeout_add(60,  gx_refresh_oscilloscope, 0);
    g_timeout_add(200, gx_survive_jack_shutdown, 0);
    g_timeout_add(600, gx_monitor_jack_ports, 0);
    g_timeout_add(500, gx_check_startup, 0);

    gtk_main();
    stop();
  }

} /* end of gx_gui namespace */