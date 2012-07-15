// #include "interface.c"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

struct Screen {
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

struct Screen init_screen () {
// Initialize screen.
  struct Screen screen;
  screen.window = initscr();
  curs_set(0);
  noecho();
  keypad(screen.window, TRUE);
  getmaxyx(screen.window, screen.rows, screen.cols);
  return screen; }

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
  char rows = 0, cols, y;
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

void init_map_screen (struct Map * map, struct Screen * screen) {
// Initialize map screen geometry.
  map->offset_y = 0;
  map->offset_x = 0;
  if (map->rows > screen->rows)
    map->offset_ymax = map->rows - screen->cols;
  else
    map->offset_ymax = map->offset_y;
  if (map->cols > screen->cols)
    map->offset_xmax = map->rows - screen->cols;
  else
   map->offset_xmax = map->offset_x; }

struct Cursor init_cursor (struct Screen * screen, struct Map * map) {
// Initialize cursor geometry.
  struct Cursor cursor;
  if (map->rows < screen->rows)
    cursor.ymax = map->rows - 1;
  else
    cursor.ymax = screen->rows - 1;
  if (map->cols < screen->cols)
    cursor.xmax = map->cols - 1;
  else
    cursor.xmax = screen->cols - 1;
  cursor.starty = cursor.ymax / 2;
  cursor.startx = cursor.xmax / 2;
  cursor.y = cursor.starty;
  cursor.x = cursor.startx;
  return cursor; }

void draw_map (struct Screen * screen, struct Map * map,
               struct Cursor * cursor) {
// Draw map onto screen; if cursor is set, highlight its position.
  int y, x, ch, map_yx[2];
  for (y = 0; y < screen->rows; y++)
    for (x = 0; x < screen->cols; x++) {
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
      mvaddch(y, x, ch); }
  refresh(); }

void nav_map_cursor (int key, struct Map * map,
                     struct Cursor * cursor) {
// Navigate map thorugh a cursor controlled by up/down/left/right keys.
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

void save_map (struct Screen * screen, struct Map * map,
               char * filename) {
// Write map to file.
  int y;
  int fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
  char size = (char) map->rows;
  write(fd, &size, 1);
  size = (char) map->cols;
  write(fd, &size, 1);
  for (y = 0; y < map->rows; y++)
    write(fd, map->map[y], map->cols);
  close(fd);
  endwin();
  exit(0); }

void fail(char * msg) {
  printf("%s\n", msg);
  exit(1); }

void usage() {
  printf("  PlomCiv map editor usage:\n"
         "  $ plomciv-map MAPFILE (open existing map file)\n"
         "  $ plomciv-map MAPFILE ROWS COLS (create new map file)\n");
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

  // Initialize map screen and cursor.
  struct Screen screen = init_screen();
  init_map_screen(&map, &screen);
  struct Cursor cursor = init_cursor(&screen, &map);

  // Map editing loop.
  int key;
  while (1) {
    draw_map(&screen, &map, &cursor);
    key = getch();
    if (key == 'q')
      break;
    else if (key == 's')
      save_map(&screen, &map, argv[1]);
    else if (33 <= key && key <= 126)
      write_char(key, &map, &cursor);
    else {
      nav_map_cursor(key, &map, &cursor); } }

  // Clean up.
  endwin();
  exit(0); }
