#include "cairo.h"
#include "gio/gio.h"
#include "glib-object.h"
#include "glib.h"
#include "glibconfig.h"
#include "gtk/gtk.h"
#include "gtk/gtkshortcut.h"
#include "pango/pango-font.h"
#include "pango/pango-layout.h"
#include "pango/pango-types.h"
#include "pango/pangocairo.h"
#include <errno.h>
#include <gtk-layer-shell/gtk-layer-shell.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct {
  GtkWidget *drawing;
  GHashTable *keys; // Map<keycode, KeyInfo>
  GIOChannel *socket;
  int socket_fd;
  guint socket_io;
} State;

typedef struct {
  guint keycode;
  char label[8]; // UTF-8 character (up to 4 bytes + null, padded)
  gboolean pressed;
} KeyInfo;

// Physical keyboard layout - maps keycodes to row/column positions
// Based on standard US QWERTY physical layout keycodes
typedef struct {
  guint keycode;
  int row;
  int col;
} KeyPosition;

// Map X11 keycodes for main letter keys to a simple 3-row layout
static const KeyPosition KEYBOARD_LAYOUT[] = {
    {24, 0, 0}, // Q
    {25, 0, 1}, // W
    {26, 0, 2}, // E
    {27, 0, 3}, // R
    {28, 0, 4}, // T
    {29, 0, 5}, // Y
    {30, 0, 6}, // U
    {31, 0, 7}, // I
    {32, 0, 8}, // O
    {33, 0, 9}, // P

    {38, 1, 0}, // A
    {39, 1, 1}, // S
    {40, 1, 2}, // D
    {41, 1, 3}, // F
    {42, 1, 4}, // G
    {43, 1, 5}, // H
    {44, 1, 6}, // J
    {45, 1, 7}, // K
    {46, 1, 8}, // L

    {52, 2, 0}, // Z
    {53, 2, 1}, // X
    {54, 2, 2}, // C
    {55, 2, 3}, // V
    {56, 2, 4}, // B
    {57, 2, 5}, // N
    {58, 2, 6}, // M
};
static const int COLS[] = {10, 9, 7};
#define ROWS ((int)(sizeof(COLS) / sizeof(COLS[0])))

static const int KEYBOARD_LAYOUT_SIZE = sizeof(KEYBOARD_LAYOUT) / sizeof(KEYBOARD_LAYOUT[0]);

static void draw_hangul_keyboard(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
  State *state = (State *)user_data;
  char *hangul_rows[][10] = {{"ㅂ", "ㅈ", "ㄷ", "ㄱ", "ㅅ", "ㅛ", "ㅕ", "ㅑ", "ㅐ", "ㅔ"},
                             {"ㅁ", "ㄴ", "ㅇ", "ㄹ", "ㅎ", "ㅗ", "ㅓ", "ㅏ", "ㅣ"},
                             {"ㅋ", "ㅌ", "ㅊ", "ㅍ", "ㅠ", "ㅜ", "ㅡ"}};

  char *qwerty_rows[][10] = {{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
                             {"A", "S", "D", "F", "G", "H", "J", "K", "L"},
                             {"Z", "X", "C", "V", "B", "N", "M"}};

  int n_rows = sizeof(hangul_rows) / sizeof(hangul_rows[0]);

  // cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  // cairo_paint(cr);
  //
  for (int r = 0; r < n_rows; r++) {
    int n_keys = 0;
    while (n_keys < 10 && hangul_rows[r][n_keys] != NULL)
      n_keys++;

    int w = width - 2 * (n_keys - 1);
    double key_width = (double)w / n_keys;
    double key_height = (double)height / n_rows;

    for (int c = 0; c < n_keys; c++) {
      double x = c * (key_width + 2);
      double y = r * (key_height + 2);

      guint keycode = 0;
      for (int i = 0; i < KEYBOARD_LAYOUT_SIZE; i++) {
        if (KEYBOARD_LAYOUT[i].row == r && KEYBOARD_LAYOUT[i].col == c) {
          keycode = KEYBOARD_LAYOUT[i].keycode;
          break;
        }
      }
      if (keycode == 0)
        continue;

      KeyInfo *key_info = g_hash_table_lookup(state->keys, GUINT_TO_POINTER(keycode));
      if (!(key_info && key_info->pressed)) {
        cairo_set_source_rgb(cr, .3, .3, .3);
        cairo_rectangle(cr, x, y, key_width, key_height);
        cairo_fill(cr);
      }

      PangoLayout *layout = pango_cairo_create_layout(cr);
      PangoFontDescription *desc = pango_font_description_from_string("Noto Sans Bold");
      pango_font_description_set_size(desc, 24 * PANGO_SCALE);

      pango_layout_set_font_description(layout, desc);
      pango_layout_set_text(layout, hangul_rows[r][c], -1);
      int text_width, text_height;
      pango_layout_get_pixel_size(layout, &text_width, &text_height);
      cairo_set_source_rgb(cr, .75, .75, .75);
      cairo_move_to(cr, x + key_width * .5 - text_width * .5, y + key_height * .5 - text_height * .5);
      pango_cairo_show_layout(cr, layout);

      pango_font_description_set_size(desc, (int)(key_height / 4) * PANGO_SCALE);
      pango_layout_set_font_description(layout, desc);
      pango_layout_set_text(layout, qwerty_rows[r][c], -1);
      pango_layout_get_pixel_size(layout, &text_width, &text_height);
      cairo_set_source_rgb(cr, .5, .5, .5);
      cairo_move_to(cr, x + key_width - text_width - 8, y + key_height - text_height - 4);
      pango_cairo_show_layout(cr, layout);

      g_object_unref(layout);
      pango_font_description_free(desc);
    }
  }
}

static void draw_callback(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
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
  ((State *)user_data)->drawing = drawing;
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing), draw_hangul_keyboard, user_data, NULL);
  gtk_widget_set_size_request(drawing, 800, 200);
  gtk_window_set_child(GTK_WINDOW(window), drawing);
  gtk_window_present(GTK_WINDOW(window));
}

static gboolean receive(int sock, void *buf, size_t size, char *name) {
  size_t r = read(sock, buf, size);
  if (r != size)
    g_printerr("Failed to read %s\n", name);
  return r != sizeof(buf);
}

// returns whether to keep listening
static gboolean handleMessage(GIOChannel *channel, GIOCondition condition, gpointer user_data) {
  State *state = (State *)user_data;

  if (condition & (G_IO_HUP | G_IO_ERR)) {
    g_print("Socket disconnected\n");
    return FALSE;
  }

  if (condition & G_IO_IN) {
    char type;
    uint32_t keysym, keycode;
    if (!receive(state->socket_fd, &type, 1, "type"))
      return FALSE;

    if (type == 'P' || type == 'R') {
      if (!receive(state->socket_fd, &keysym, sizeof(keysym), "keysym") ||
          !receive(state->socket_fd, &keycode, sizeof(keycode), "keycode"))
        return FALSE;

      KeyInfo *key = g_hash_table_lookup(state->keys, GUINT_TO_POINTER(keycode));
      if (!key) {
        key = g_new(KeyInfo, 1);
        key->keycode = keycode;
        g_hash_table_insert(state->keys, GUINT_TO_POINTER(keycode), key);
      }
      key->pressed = type == 'P';
      gtk_widget_queue_draw(state->drawing);
    } else {
      g_printerr("Unknown message type: 0x%02x\n", type);
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean connect_to_fcitx5(State *state) {
  const char *runtime_dir = g_getenv("XDG_RUNTIME_DIR");
  if (!runtime_dir)
    runtime_dir = "/tmp";
  char socket_path[512];
  snprintf(socket_path, sizeof(socket_path), "%s/fkwhud.sock", runtime_dir);

  state->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (state->socket_fd < 0) {
    g_printerr("Failed to create socket: %s\n", strerror(errno));
    return FALSE;
  }

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  if (connect(state->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    g_printerr("Failed to connect to fcitx5 addon at %s: %s\n", socket_path, strerror(errno));
    g_printerr("Make sure the fcitx5-fkwhud addon is installed and fcitx5 is running.\n");
    close(state->socket_fd);
    state->socket_fd = -1;
    return FALSE;
  }
  g_print("Connected to fcitx5 addon at %s\n", socket_path);

  state->socket = g_io_channel_unix_new(state->socket_fd);
  g_io_channel_set_encoding(state->socket, NULL, NULL);
  g_io_channel_set_buffered(state->socket, FALSE);
  state->socket_io = g_io_add_watch(state->socket, G_IO_IN | G_IO_HUP | G_IO_ERR, handleMessage, state);
  return TRUE;
}

int main(int argc, char **argv) {
  State *state = g_new(State, 1);
  state->socket = NULL;
  state->socket_fd = -1;
  state->socket_io = 0;
  state->keys = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
  if (!connect_to_fcitx5(state)) {
    g_printerr("Warning: Could not connect to fcitx5. Key events will not be received.\n");
  }

  GtkApplication *app = gtk_application_new(NULL, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(app_activate), state);
  return g_application_run(G_APPLICATION(app), argc, argv);
}
