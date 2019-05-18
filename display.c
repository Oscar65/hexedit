/* hexedit -- Hexadecimal Editor for Binary Files
   Copyright (C) 1998 Pixel (Pascal Rigaux)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.*/
#include "hexedit.h"

int move_cursor(INT delta)
{
  return set_cursor(base + cursor + delta);
}

int set_cursor(INT loc)
{
  if (loc < 0 && base % lineLength)
    loc = 0;

  if (!tryloc(loc))
    return FALSE;

  if (loc < base) {
    if (loc - base % lineLength < 0)
      set_base(0);
    else if (!move_base(myfloor(loc - base % lineLength, lineLength) + base % lineLength - base))
      return FALSE;
    cursor = loc - base;
  } else if (loc >= base + page) {
    if (!move_base(myfloor(loc - base % lineLength, lineLength) + base % lineLength - page + lineLength - base))
      return FALSE;
    cursor = loc - base;
  } else if (loc > base + nbBytes) {
    return FALSE;
  } else
    cursor = loc - base;

  if (mark_set)
    updateMarked();

  return TRUE;
}

int move_base(INT delta)
{
  if (mode == bySector) {
    if (delta > 0 && delta < page)
      delta = page;
    else if (delta < 0 && delta > -page)
      delta = -page;
  }
  return set_base(base + delta);
}

int set_base(INT loc)
{
  if (loc < 0) loc = 0;

  if (!tryloc(loc)) return FALSE;
  base = loc;
  readFile();

  if (mode != bySector && nbBytes < page - lineLength && base != 0) {
    base -= myfloor(page - nbBytes - lineLength, lineLength);
    if (base < 0) base = 0;
    readFile();
  }

  if (cursor > nbBytes) cursor = nbBytes;
  return TRUE;
}


int computeLineSize(void) { return computeCursorXPos(lineLength - 1, 0) + 1; }
int computeCursorXCurrentPos(void) { return computeCursorXPos(cursor, hexOrAscii); }
int computeCursorXPos(INT cursor, int hexOrAscii)
{
  int r = 11;
  int x = cursor % lineLength;
  int h = (hexOrAscii ? x : lineLength - 1);
  int szBuffer = 100;
  char buffer[szBuffer];
  r += snprintf(buffer, szBuffer, "%08llX", base+cursor-x) - 8;
  r += normalSpaces * (h % blocSize) + (h / blocSize) * (normalSpaces * blocSize + 1) + (hexOrAscii && cursorOffset);

  if (!hexOrAscii) r += x + normalSpaces + 1;

  return r;
}



/*******************************************************************************/
/* Curses functions */
/*******************************************************************************/
static void handleSigWinch(int sig)
{
  /*Close and refresh window to get new size*/
  endwin();
  refresh();

  /*Reset to trigger recalculation*/
  lineLength = 0;

  clear();
  initDisplay();
  readFile();
  display();
}

void initCurses(void)
{
  struct sigaction sa;
  if ((initscr()) == NULL) {
    fprintf(stderr, "Error calling ncurses initscr.\n");
    exit(EXIT_FAILURE);
  }

  memset(&sa, 0, sizeof(struct sigaction));
  sa.sa_handler = handleSigWinch;
  sigaction(SIGWINCH, &sa, NULL);

#ifdef HAVE_COLORS
  if (colored) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_RED, -1);   /* null zeros */
    init_pair(2, COLOR_GREEN, -1); /* control chars */
    init_pair(3, COLOR_BLUE, -1);  /* extended chars */
  }
  init_pair(4, COLOR_BLUE, COLOR_YELLOW); /* current cursor position*/
#endif

  initDisplay();
}

void initDisplay(void)
{
  refresh();
  raw();
  noecho();
  keypad(stdscr, TRUE);

  if (mode == bySector) {
    lineLength = modes[bySector].lineLength;
    page = modes[bySector].page;
    page = myfloor((LINES - 1) * lineLength, page);
    blocSize = modes[bySector].blocSize;
    if (computeLineSize() > COLS) DIE("%s: term is too small for sectored view (width)\n");
    if (page == 0) DIE("%s: term is too small for sectored view (height)\n");
  } else { /* mode == maximized */
    if (LINES <= 4) DIE("%s: term is too small (height)\n");

    blocSize = modes[maximized].blocSize;
    if (lineLength == 0) {
      /* put as many "blocSize" as can fit on a line */
      for (lineLength = blocSize; computeLineSize() <= COLS; lineLength += blocSize);
      lineLength -= blocSize;
      if (lineLength == 0) DIE("%s: term is too small (width)\n");
    } else {
      if (computeLineSize() > COLS)
        DIE("%s: term is too small (width) for selected line length\n");
    }
    page = lineLength * (LINES - 1);
  }
  colsUsed = computeLineSize();
  buffer = realloc(buffer,page);
  bufferAttr = realloc(bufferAttr,page * sizeof(*bufferAttr));
}

void exitCurses(void)
{
  close(fd);
  clear();
  
  if (refresh() == ERR) {
    fprintf(stderr, "%s:%d Error calling ncurses refresh.\n", __FILE__, __LINE__);
  }
  if (delwin(stdscr) == ERR) {
    fprintf(stderr, "Error calling ncurses delwin.\n");
  }
  if (endwin() == ERR) {
    fprintf(stderr, "Error calling ncurses endwin.\n");
    exit(EXIT_FAILURE);
  }
}

void display(void)
{
  int i;

  for (i = 0; i < nbBytes; i += lineLength) {
    move(i / lineLength, 0);
    displayLine(i, nbBytes);
  }

  for (; i < page; i += lineLength) {
    int j;
    move(i / lineLength, 0);
    clrtoeol();
    move(i / lineLength, 0);
    PRINTW(("%08llX", (base + i)));
  }
  attrset(NORMAL);
  move(LINES - 1, 0);
  int szBuffer = 100;
  char buffer[szBuffer];
  int len = snprintf(buffer, szBuffer, "%08llX", base + i) - 8;
  for (i = 0; i < colsUsed + len; i++) printw("-");
  clrtoeol();
  move(LINES - 1, 0);
  if (isReadOnly) i = '%';
  else if (edited) i = '*';
  else i = '-';
  printw("%c%c 0x%llX", i, i, base + cursor);
  if (MAX(fileSize, lastEditedLoc)) printw("/0x%llX", getfilesize());
  printw("--%i%%", 100 * (base + cursor + getfilesize()/200) / getfilesize() );
  if (mode == bySector) printw("--sector %lld", (base + cursor) / SECTOR_SIZE);
  if (mark_set) printw("--sel 0x%llX/0x%llX--size 0x%llX", mark_min, mark_max, mark_max - mark_min + 1);
  printw(" %s", baseName);
  move(cursor / lineLength, computeCursorXCurrentPos());
}

void displayLine(size_t offset, size_t max)
{
  size_t i;
  int mark_color=0;
#ifdef HAVE_COLORS
  mark_color = COLOR_PAIR(4) | A_BOLD;
#endif
  PRINTW(("%08llX   ", (base + offset)));
  for (i = offset; i < offset + lineLength; i++) {
    if (i > offset) MAXATTRPRINTW(bufferAttr[i] & MARKED, (((i - offset) % blocSize) ? " " : "  "));
    if (i < max) {
      ATTRPRINTW(
#ifdef HAVE_COLORS
		 (!colored ? 0 :
      (cursor == i && hexOrAscii == 0 ? mark_color :
		  buffer[i] == 0 ? COLOR_PAIR(1) :
		  buffer[i] < ' ' ? COLOR_PAIR(2) : 
		  buffer[i] >= 127 ? COLOR_PAIR(3) : 0)
      ) |
#endif
		 bufferAttr[i], ("%02X", buffer[i]));
    }
    else PRINTW(("  "));
  }
  PRINTW(("  "));
  for (i = offset; i < offset + lineLength; i++) {
    if (i >= max) ATTRPRINTW(bufferAttr[i-1], (" "));
    else if (buffer[i] >= ' ' && buffer[i] < 127) ATTRPRINTW((cursor == i && hexOrAscii==1 ? mark_color : 0) | bufferAttr[i], ("%c", buffer[i]));
      else ATTRPRINTW((cursor == i && hexOrAscii == 1 ? mark_color : 0) | bufferAttr[i], ("."));
  }
  PRINTW((" "));
  clrtoeol();
}

void clr_line(int line) { move(line, 0); clrtoeol(); }

void displayCentered(const char *msg, int line)
{
  clr_line(line);
  move(line, (COLS - strlen(msg)) / 2);
  PRINTW(("%s", msg));
}

void displayOneLineMessage(const char *msg)
{
  int center = page / lineLength / 2;
  clr_line(center - 1);
  clr_line(center + 1);
  displayCentered(msg, center);
}

void displayTwoLineMessage(const char *msg1, const char *msg2)
{
  int center = page / lineLength / 2;
  clr_line(center - 2);
  clr_line(center + 1);
  displayCentered(msg1, center - 1);
  displayCentered(msg2, center);
}

void displayMessageAndWaitForKey(const char *msg)
{
  displayTwoLineMessage(msg, pressAnyKey);
  getch();
}

int displayMessageAndGetString(const char *msg, char **last, char *p, size_t p_size)
{
  int ret = TRUE;

  displayOneLineMessage(msg);
  ungetstr(*last, p_size);
  echo();
  getnstr(p, p_size - 1);
  noecho();
  if (*p == '\0') {
    if (*last) strcpy(p, *last); else ret = FALSE;
  } else {
    FREE(*last);
    *last = strdup(p);
  }
  return ret;
}

void ungetstr(char *s, size_t p_size)
{
  wchar_t ws[p_size];
  wchar_t *w;
  bool utf8 = false;

  if (s) {
    swprintf(ws, p_size, L"%hs", s);
    int i;
    for (i=0; i<wcslen(ws); i++) {
      if (ws[i] > 127) utf8 =true;
    }
    if ((wcslen(ws) < 75) && (!utf8)) {
      for (w = ws + wcslen(ws) - 1; w >= ws; w--) {
        if (unget_wch(*w) != OK) {
          printf("unget_wch ERROR!");
          break;
        }
      }
    }
  } 
}

int get_number(INT *i)
{
  int err;
  char tmp[BLOCK_SEARCH_SIZE];
  echo();
  getnstr(tmp, BLOCK_SEARCH_SIZE - 1);
  noecho();
  if (strbeginswith(tmp, "0x"))
    err = sscanf(tmp + strlen("0x"), "%llx", i);
  else
    err = sscanf(tmp, "%lld", i);
  return err == 1;
}
