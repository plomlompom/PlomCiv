#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#define LINEBUF_MAX 1024

struct Window {
  WINDOW * window;
  int rows;
  int cols; };

struct Map {
  char ** map;
  int rows;
  int cols;
  int offset_y;
  int offset_x;
  int offset_ymax;
  int offset_xmax; };

struct Cursor {
  int y;
  int x;
  int midy;
  int midx;
  int ymax;
  int xmax;
  int select_y;
  int select_x; };

struct Window init_window(int rows, int cols, int y, int x) {
// Initialize new Window struct.
  struct Window window;
  window.rows = rows;
  window.cols = cols;
  window.window = newwin(window.rows, window.cols, y, x);
  return window; }

struct Map new_map (int rows, int cols) {
// Build new map matrix.
  struct Map map;
  int y, x;
  map.map = malloc(rows * sizeof(char *));
  for (y = 0; y < rows; y++) {
    map.map[y] = malloc(cols * sizeof(char));
    for (x = 0; x < cols; x++)
      map.map[y][x] = '~'; } // Default value for map cells.
  map.rows = rows;
  map.cols = cols;
  return map; }

struct Map read_map (char * filename) {
// Read in map matrix from file.
  int fd = open(filename, O_RDONLY);
  struct Map map;
  unsigned char rows = 0, cols, y;
  read(fd, &rows, 1);
  read(fd, &cols, 1);
  map.map = malloc(rows * sizeof(char *));
  for (y = 0; y < rows; y++) {
    map.map[y] = malloc(cols * sizeof(char));
    read(fd, map.map[y], cols); }
  map.rows = rows;
  map.cols = cols;
  close(fd);
  return map; }

void init_mapwindow (struct Map * map, struct Window * window) {
// Initialize map window geometry.
  map->offset_y = 0;
  map->offset_x = 0;
  if (map->rows > window->rows)
    map->offset_ymax = map->rows - window->rows;
  else
    map->offset_ymax = map->offset_y;
  if (map->cols > window->cols)
    map->offset_xmax = map->cols - window->cols;
  else
    map->offset_xmax = map->offset_x; }

struct Cursor init_cursor (struct Window * window, struct Map * map) {
// Initialize cursor geometry.
  struct Cursor cursor;
  if (map->rows < window->rows)
    cursor.ymax = map->rows - 1;
  else
    cursor.ymax = window->rows - 1;
  if (map->cols < window->cols)
    cursor.xmax = map->cols - 1;
  else
    cursor.xmax = window->cols - 1;
  cursor.midy = cursor.ymax / 2;
  cursor.midx = cursor.xmax / 2;
  cursor.y = cursor.midy;
  cursor.x = cursor.midx;
  return cursor; }

void draw_map (struct Window * window, struct Map * map,
               struct Cursor * cursor, char select_type) {
// Draw map onto window; if cursor is set, highlight its position.
  int y, x, ch, map_yx[2];
  for (y = 0; y < window->rows; y++)
    for (x = 0; x < window->cols; x++) {
      map_yx[0] = y + map->offset_y;
      map_yx[1] = x + map->offset_x;
      ch = ' ';
      if (map_yx[0] < map->rows && map_yx[1] < map->cols
          && map->map[map_yx[0]][map_yx[1]])
        ch = map->map[map_yx[0]][map_yx[1]];
      if (cursor && y == cursor->y &&
         ( (!select_type && x == cursor->x) || select_type ) ) {
          cursor->select_y = map_yx[0];
          if (!select_type)
            cursor->select_x = map_yx[1];
          ch = ch | A_REVERSE; }
      mvwaddch(window->window, y, x, ch); }
  wrefresh(window->window); }

void nav_map_cursor (int key, struct Map * map,
                     struct Cursor * cursor, char select_type) {
// Navigate map through a cursor controlled by up/down/left/right keys.
  if      (key == KEY_UP) {
    if      (cursor->y > cursor->midy)         cursor->y--;
    else if (map->offset_y > 0)                map->offset_y--;
    else if (cursor->y > 0)                    cursor->y--; }
  else if (key == KEY_DOWN) {
    if      (cursor->y < cursor->midy)         cursor->y++;
    else if (map->offset_y < map->offset_ymax) map->offset_y++;
    else if (cursor->y < cursor->ymax)         cursor->y++; }
  else if (!select_type) {
    if (key == KEY_LEFT) {
      if      (cursor->x > cursor->midx)         cursor->x--;
      else if (map->offset_x > 0)                map->offset_x--;
      else if (cursor->x > 0)                    cursor->x--; }
    else if (key == KEY_RIGHT) {
      if      (cursor->x < cursor->midx)         cursor->x++;
      else if (map->offset_x < map->offset_xmax) map->offset_x++;
      else if (cursor->x < cursor->xmax)         cursor->x++; } }
  else {
    if (key == KEY_LEFT && map->offset_x > 0)
      map->offset_x--;
    else if (key == KEY_RIGHT && map->offset_x < map->offset_xmax)
      map->offset_x++; } }

void write_char (int key, struct Map * map, struct Cursor * cursor) {
// Write char to map at cursor selection.
  map->map[cursor->select_y][cursor->select_x] = key; }

void update_status (struct Window * statuswin, char * text) {
// Update status line with text.
  int x;
  for (x = 0; x < statuswin->cols; x++)
    mvwaddch(statuswin->window, 0, x, ' ');
  mvwprintw(statuswin->window, 0, 0, text);
  wrefresh(statuswin->window); }

void save_as (struct Window * window, char filename[256]) {
// Replace file name in status line.
  int cursor = 0, max_x = 0, key, ch, x;
  char tmp[256], intro[] = "File name: ";
  int len_intro = sizeof(intro);

  // Initialize file name editor.
  for (x = 0; x < window->cols; x++)
    mvwaddch(window->window, 0, x, ' ');
  mvwprintw(window->window, 0, 0, intro);
  for (x = 0; x < 256; x++)
    filename[x] = 0;

  // Loop: print current filename, collect key press.
  while (1) {
    for (x = 0; x < 256; x++) {
      if (filename[x]) ch = filename[x];
      else             ch = ' ';
      if (x == cursor) ch = ch | A_REVERSE;
      mvwaddch(window->window, 0, (len_intro + x) - 1, ch);
      wrefresh(window->window); }
    key = wgetch(window->window);

    // Enter printable ASCII chars.
    if ((32 <= key && key <= 126)) {
      memcpy(tmp, filename, sizeof(tmp));
      for (x = 0; x < cursor; x++)
        filename[x] = tmp[x];
      filename[cursor] = key;
      for (x = cursor; x < (sizeof(tmp) - 1); x++)
        filename[x + 1] = tmp[x];
      cursor++;
      max_x++; }

    // Delete char before cursor with BACKSPACE.
    else if (key == KEY_BACKSPACE && cursor > 0) {
      memcpy(tmp, filename, sizeof(tmp));
      for (x = 0; x < cursor - 1; x++)
        filename[x] = tmp[x];
      for (x = cursor; x < (sizeof(tmp) - 1); x++)
        filename[x - 1] = tmp[x];
      filename[x++] = 0;
      cursor--;
      max_x--; }

    // Navigate cursor left / right.
    else if (key == KEY_LEFT  && cursor > 0)     cursor--;
    else if (key == KEY_RIGHT && cursor < max_x) cursor++;

    // Finish with ENTER.
    else if (key == '\n')
      break; } }

void save_map (struct Window * window, struct Map * map,
               char * filename, char * default_text) {
// Write map to file.
  int x, y;
  update_status(window, "Saving map ...");
  int fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  char size = (char) map->rows;
  write(fd, &size, 1);
  size = (char) map->cols;
  write(fd, &size, 1);
  for (y = 0; y < map->rows; y++)
    write(fd, map->map[y], map->cols);
  close(fd);
  update_status(window, "Map saved. Hit some key to continue.");
  wgetch(window->window);
  update_status(window, default_text); }

void drop_map (struct Map * map) {
// Free memory for map.
  int y;
  for (y = 0; y < map->rows; y++)
    free(map->map[y]);
  free(map->map); }

struct Map map_from_lines (char ** lines, int rows) {
// Build map from string lines (of possibly unequal length).
  int y, x;
  struct Map map;
  map.rows = rows;

  // Determine map column number from max line length.
  map.cols = 0;
  for (y = 0; y < map.rows; y++) {
    x = strlen(lines[y]);
    if (x > map.cols)
      map.cols = x; }
  map.cols = map.cols - 1;

  // Write lines to map.
  map.map = malloc(map.rows * sizeof(char *));
  for (y = 0; y < map.rows; y++)
    map.map[y] = calloc(map.cols, sizeof(char));
  for (y = 0; y < map.rows; y++)
    memcpy(map.map[y], lines[y], strlen(lines[y]) - 1);
  return map; }

char select_terrain (struct Window * window, struct Window * screen, char brush) {
// Select terrain to paint with cursor.

  // Read in terrain file.
  FILE * file = fopen("terrains", "r");
  int i, ii;
  char buf[LINEBUF_MAX], ** lines;
  for (i = 0; fgets(buf, LINEBUF_MAX, file); i++);
  rewind(file);
  lines = malloc(i * sizeof(char *));
  for (i = 0; fgets(buf, LINEBUF_MAX, file); i++) {
    lines[i] = calloc(strlen(buf), sizeof(char));
    memcpy(lines[i], buf, strlen(buf)); }
  fclose(file);
  struct Map map = map_from_lines(lines, i);
  for (ii = 0; ii < i; ii++)
    free(lines[ii]);
  free(lines);

  // Initialization of map, cursor.
  init_mapwindow(&map, window);
  struct Cursor cursor = init_cursor(window, &map);
  cursor.y = 0;

  // Terrain selection loop.
  int key;
  while (1) {
    draw_map(window, &map, &cursor, 1);
    key = wgetch(window->window);
    if (key == 'q')
      break;
    else if (key == '\n') {
      brush = map.map[cursor.select_y][0];
      break; }
    nav_map_cursor(key, &map, &cursor, 1); }

  // Finish only after freeing allocated memory.
  drop_map(&map);
  return brush; }

void fail(char * msg) {
// Print error message and exit.
  printf("%s\n", msg);
  exit(1); }

void usage() {
// Help message.
  printf("PlomCiv map editor usage:\n"
         "  $ plomciv-map MAPFILE (open existing map file)\n"
         "  $ plomciv-map MAPFILE ROWS COLS (create new map file)\n"
         "Key bindings:\n"
         "  arrow keys: move cursor\n"
         "           q: quit\n"
         "           s: save to current file name\n"
         "           S: save to new file name\n"
         "           x: write currently selected terrain\n"
         "           X: change terrain selection\n");
  exit(0); }

int main (int argc, char *argv[]) {

  // Try to initialize map from command line arguments.
  struct Map map;
  if (argc != 2 && argc != 4 ||
      !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
    usage();
  else if (argc == 2) {
    map = read_map(argv[1]);
    if (!map.rows)
      fail("Unable to open map."); }
  else {
    if (atoi(argv[2]) > 256 || atoi(argv[3]) > 256)
      fail("Number of rows / columns must not exceed 256.");
    if (fopen(argv[1], "r"))
      fail("Map file already exists. Delete first.");
    map = new_map(atoi(argv[2]), atoi(argv[3])); }

  // Initialize screen.
  struct Window screen;
  screen.window = initscr();
  getmaxyx(screen.window, screen.rows, screen.cols);
  curs_set(0);
  cbreak();
  noecho();

  // Initialize status line.
  char brush = '~';
  struct Window status = init_window(1, screen.cols, 0, 0);
  char * status_msg = calloc(status.cols, sizeof(char));
  snprintf(status_msg, status.cols, "[%c] PlomCiv map editor: %s", brush, argv[1]);
  update_status(&status, status_msg);
  keypad(status.window, TRUE);

  // Initialize map window and map window cursor.
  struct Window mapwindow = init_window(screen.rows - 1, screen.cols, 1, 0);
  keypad(mapwindow.window, TRUE);
  init_mapwindow(&map, &mapwindow);
  struct Cursor cursor = init_cursor(&mapwindow, &map);

  // Map editing loop.
  int key;
  char tmp_str[256];
  while (1) {
    draw_map(&mapwindow, &map, &cursor, 0);
    key = wgetch(mapwindow.window);
    if (key == 'q')
      break;
    else if (key == 's') {
      draw_map(&mapwindow, &map, &cursor, 0);
      save_map(&status, &map, argv[1], status_msg); }
    else if (key == 'S') {
      draw_map(&mapwindow, &map, 0, 0);
      save_as(&status, tmp_str);
      if (strlen(tmp_str))
        argv[1] = tmp_str;
      snprintf(status_msg, status.cols, "PlomCiv map editor: %s", argv[1]);
      save_map(&status, &map, argv[1], status_msg); }
    else if (key == 'x')
      write_char(brush, &map, &cursor);
    else if (key == 'X') {
      brush = select_terrain(&mapwindow, &screen, brush);
      snprintf(status_msg, status.cols, "[%c] PlomCiv map editor: %s", brush, argv[1]);
      update_status(&status, status_msg); }
    else
      nav_map_cursor(key, &map, &cursor, 0); }

  // Clean up.
  endwin();
  exit(0); }
