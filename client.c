#include <gtk-layer-shell/gtk-layer-shell.h>

static void draw_hangul_keyboard(GtkDrawingArea *area, cairo_t *cr, int width,
                                 int height, gpointer user_data) {
  char *hangul_rows[][10] = {
      {"ㅂ", "ㅈ", "ㄷ", "ㄱ", "ㅅ", "ㅛ", "ㅕ", "ㅑ", "ㅐ", "ㅔ"},
      {"ㅁ", "ㄴ", "ㅇ", "ㄹ", "ㅎ", "ㅗ", "ㅓ", "ㅏ", "ㅣ"},
      {"ㅋ", "ㅌ", "ㅊ", "ㅍ", "ㅠ", "ㅜ", "ㅡ"}};

  char *qwerty_rows[][10] = {{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
                             {"A", "S", "D", "F", "G", "H", "J", "K", "L"},
                             {"Z", "X", "C", "V", "B", "N", "M"}};

  int n_rows = sizeof(hangul_rows) / sizeof(hangul_rows[0]);

  for (int r = 0; r < n_rows; r++) {
    int n_keys = 0;
    while (n_keys < 10 && hangul_rows[r][n_keys] != NULL) {
      n_keys++;
    }

    double key_width = (double)width / n_keys;
    double key_height = (double)height / n_rows;

    for (int i = 0; i < n_keys; i++) {
      double x = i * key_width;
      double y = r * key_height;

      cairo_set_source_rgb(cr, .3, .3, .3);
      cairo_rectangle(cr, x + 2, y + 2, key_width - 4, key_height - 4);
      cairo_fill(cr);

      cairo_rectangle(cr, x + 2, y + 2, key_width - 4, key_height - 4);
      cairo_stroke(cr);

      PangoLayout *layout = pango_cairo_create_layout(cr);
      // TODO: Use a smarter way to get a reasonably decent size
      PangoFontDescription *desc =
          pango_font_description_from_string("Noto Sans CJK KR Bold 24");

      pango_layout_set_font_description(layout, desc);
      pango_layout_set_text(layout, hangul_rows[r][i], -1);
      int text_width, text_height;
      pango_layout_get_pixel_size(layout, &text_width, &text_height);
      cairo_set_source_rgb(cr, .75, .75, .75);
      cairo_move_to(cr, x + key_width * .5 - text_width * .5,
                    y + key_height * .5 - text_height * .5);
      pango_cairo_show_layout(cr, layout);

      pango_font_description_set_size(desc,
                                      (int)(key_height / 4) * PANGO_SCALE);
      pango_layout_set_font_description(layout, desc);
      pango_layout_set_text(layout, qwerty_rows[r][i], -1);
      pango_layout_get_pixel_size(layout, &text_width, &text_height);
      cairo_set_source_rgb(cr, .5, .5, .5);
      cairo_move_to(cr, x + key_width - text_width - 8,
                    y + key_height - text_height - 4);
      pango_cairo_show_layout(cr, layout);

      g_object_unref(layout);
      pango_font_description_free(desc);
    }
  }
}

static void draw_callback(GtkDrawingArea *area, cairo_t *cr, int width,
                          int height, gpointer user_data) {
  cairo_set_source_rgb(cr, .3, .3, .3);
  cairo_rectangle(cr, 0, 0, width, height);
  cairo_fill(cr);
}

static void app_activate(GtkApplication *app, gpointer user_data) {
  int height = 200;
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
  gtk_window_set_default_size(GTK_WINDOW(window), 800, height);

  gtk_layer_init_for_window(GTK_WINDOW(window));
  gtk_layer_set_layer(GTK_WINDOW(window), GTK_LAYER_SHELL_LAYER_BOTTOM);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_layer_set_anchor(GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);
  gtk_layer_set_exclusive_zone(GTK_WINDOW(window), height);

  GtkWidget *drawing = gtk_drawing_area_new();
  gtk_widget_set_size_request(drawing, 800, 200);
  gtk_window_set_child(GTK_WINDOW(window), drawing);

  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing),
                                 draw_hangul_keyboard, NULL, NULL);

  gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(app_activate), NULL);
  return g_application_run(G_APPLICATION(app), argc, argv);
}
