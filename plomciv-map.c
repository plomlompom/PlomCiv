#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

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
  int starty;
  int startx;
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
    map->offset_xmax = map->rows - window->cols;
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
  cursor.starty = cursor.ymax / 2;
  cursor.startx = cursor.xmax / 2;
  cursor.y = cursor.starty;
  cursor.x = cursor.startx;
  return cursor; }

void draw_map (struct Window * window, struct Map * map,
               struct Cursor * cursor) {
// Draw map onto window; if cursor is set, highlight its position.
  int y, x, ch, map_yx[2];
  for (y = 0; y < window->rows; y++)
    for (x = 0; x < window->cols; x++) {
      map_yx[0] = y + map->offset_y;
      map_yx[1] = x + map->offset_x;
      if (map_yx[0] >= map->rows || map_yx[1] >= map->cols)
        ch = ' ';
      else if (map->map[map_yx[0]][map_yx[1]])
        ch = map->map[map_yx[0]][map_yx[1]];
      else
        ch = '%';
      if (cursor && y == cursor->y && x == cursor->x) {
        cursor->select_y = map_yx[0];
        cursor->select_x = map_yx[1];
        ch = ch | A_REVERSE; }
      mvwaddch(window->window, y, x, ch); }
  wrefresh(window->window); }

void nav_map_cursor (int key, struct Map * map,
                     struct Cursor * cursor) {
// Navigate map through a cursor controlled by up/down/left/right keys.
  if      (key == KEY_UP) {
    if      (cursor->y > cursor->starty)       cursor->y--;
    else if (map->offset_y > 0)                map->offset_y--;
    else if (cursor->y > 0)                    cursor->y--; }
  else if (key == KEY_LEFT) {
    if      (cursor->x > cursor->startx)       cursor->x--;
    else if (map->offset_x > 0)                map->offset_x--;
    else if (cursor->x > 0)                    cursor->x--; }
  else if (key == KEY_DOWN) {
    if      (cursor->y < cursor->starty)       cursor->y++;
    else if (map->offset_y < map->offset_ymax) map->offset_y++;
    else if (cursor->y < cursor->ymax)         cursor->y++; }
  else if (key == KEY_RIGHT) {
    if      (cursor->x < cursor->startx)       cursor->x++;
    else if (map->offset_x < map->offset_xmax) map->offset_x++;
    else if (cursor->x < cursor->xmax)         cursor->x++; } }

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
         "           s: save and quit\n"
         "All other free keys that can type ASCII characters"
         "can be used to type into / edit the map.\n");
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

  // Initialize screen, status line, map window and cursor.
  struct Window screen;
  screen.window = initscr();
  getmaxyx(screen.window, screen.rows, screen.cols);
  curs_set(0);
  cbreak();
  noecho();
  struct Window status = init_window(1, screen.cols, 0, 0);
  char * status_msg = calloc(status.cols, sizeof(char));
  snprintf(status_msg, status.cols, "PlomCiv map editor: %s", argv[1]);
  update_status(&status, status_msg);
  struct Window mapwindow = init_window(screen.rows - 1, screen.cols, 1, 0);
  keypad(mapwindow.window, TRUE);
  init_mapwindow(&map, &mapwindow);
  struct Cursor cursor = init_cursor(&mapwindow, &map);

  // Map editing loop.
  int key;
  while (1) {
    draw_map(&mapwindow, &map, &cursor);
    key = wgetch(mapwindow.window);
    if (key == 'q')
      break;
    else if (key == 's')
      save_map(&status, &map, argv[1], status_msg);
    else if (33 <= key && key <= 126)
      write_char(key, &map, &cursor);
    else {
      nav_map_cursor(key, &map, &cursor); } }

  // Clean up.
  endwin();
  exit(0); }
