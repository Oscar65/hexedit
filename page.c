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
#include <stdlib.h>

void setToChar(int i, unsigned char c)
{
  if (i >= nbBytes) {      /* If appending, always mark as modified */
    buffer[i] = c;
    bufferAttr[i] |= MODIFIED;
    addToEdited(base + i, 1, &c);
    nbBytes = i+1;
  } else if (buffer[i] != c) {
    buffer[i] = c;
    bufferAttr[i] |= MODIFIED;
    addToEdited(base + i, 1, &c);
  }
}

/*******************************************************************************/
/* Pages handling functions */
/*******************************************************************************/
void updatelastEditedLoc(void)
{
  typePage *p;
  lastEditedLoc = 0;
  for (p = edited; p; p = p->next) {
    if (p->base + p->size > lastEditedLoc)
      lastEditedLoc = p->base + p->size;
  }
}

void discardEdited(void)
{
  typePage *p, *q;
  for (p = edited; p; p = q) {
    q = p->next;
    freePage(p);
  } 
  edited = NULL; 
  lastEditedLoc = 0;
  if (base + cursor > biggestLoc) set_cursor(biggestLoc);
  if (mark_max >= biggestLoc) mark_max = biggestLoc - 1;
}

void addToEdited(INT base, INT size, unsigned char *vals)
{
  typePage *p, *q = NULL;
  for (p = edited; p; q = p, p = p->next) {
    if (base + size <= p->base) break;
    if (base <= p->base && p->base + p->size <= base + size) {
      if (q) q->next = p->next; else edited = p->next;
      freePage(p); p = q;
      if (q == NULL) {
	p = edited;
	if (p && p->base >= base + size) break;
	if (p == NULL) break;
      }
    }
  }

  if (q && base <= q->base + q->size && q->base <= base + size && llabs(q->base - base) < MAX_SIZE_PAGE) {
    /* chevauchement (?? how to say it in english ??) overlap? */
    INT min, max;
    unsigned char *s;
    min = MIN(q->base, base);
    if (p && base + size == p->base) {
      max = p->base + p->size;
      s = malloc(max - min);
      if (s != NULL) {
        memcpy(s + q->base - min, q->vals, q->size);
        memcpy(s + base - min, vals, size);
        memcpy(s + p->base - min, p->vals, p->size);
        free(q->vals); q->vals = s;
        q->next = p->next;
        freePage(p);
      } else {
        displayMessageAndWaitForKey("Can't allocate that much memory");
        return;
      }
    } else {
      max = MAX(q->base + q->size, base + size);
      s = malloc(max - min);
      if (s != NULL) {
        memcpy(s + q->base - min, q->vals, q->size);
        memcpy(s + base - min, vals, size);
        free(q->vals); q->vals = s;
      } else {
        displayMessageAndWaitForKey("Can't allocate that much memory");
        return;
      }
    }
    q->base = min;
    q->size = max - min;
  } else if (p && base + size == p->base && llabs(p->base - base) < MAX_SIZE_PAGE) {
    unsigned char *s = malloc(p->base + p->size - base);
    if (s != NULL) {
      memcpy(s, vals, size);
      memcpy(s + p->base - base, p->vals, p->size);
      free(p->vals); p->vals = s;
      p->size = p->base + p->size - base;
      p->base = base;
    } else {
      displayMessageAndWaitForKey("Can't allocate that much memory");
      return;
    }
  } else {
    INT i;
    for (i = base; i < base + size; i = i + MAX_SIZE_PAGE) {
      size_t pageSize = ((i + MAX_SIZE_PAGE) - (base + size) < MAX_SIZE_PAGE) ? base + size - i : MAX_SIZE_PAGE;
      typePage *r = newPage(i, pageSize);
      if (r) {
        if (r->vals) {
          memcpy(r->vals, vals + (i - base), pageSize);
          if (q) q->next = r; else edited = r;
          r->next = p;
          q = r;
        } else {
          displayMessageAndWaitForKey("Can't allocate that much memory");
          return;
        }
      } else {
        displayMessageAndWaitForKey("Can't allocate that much memory");
        return;
      }
      if (pageSize < MAX_SIZE_PAGE) break;
    }
  }
  updatelastEditedLoc();
}

void removeFromEdited(INT base, INT size)
{
  typePage *p, *q = NULL;
  for (p = edited; p; p ? (q = p, p = p->next) : (q = NULL, p = edited)) {
    if (base + size <= p->base) break;
    if (base <= p->base) {
      if (p->base + p->size <= base + size) {
	if (q) q->next = p->next; else edited = p->next;
  	freePage(p);
	p = q;
      } else {
	p->size -= base + size - p->base;
	memmove(p->vals, p->vals + base + size - p->base, p->size);
	p->base = base + size;
      }
    } else if (p->base + p->size <= base + size) {
      if (base < p->base + p->size) p->size -= p->base + p->size - base;      
    } else {
      q = newPage(base + size, p->base + p->size - base - size);
      if (q != NULL) {
        memcpy(q->vals, p->vals + base + size - p->base, q->size);
        q->next = p->next;
        p->next = q;      
        p->size -= p->base + p->size - base;
        break;
      } else {
        displayMessageAndWaitForKey("Can't allocate that much memory");
        return;
      }
    }
  }
  updatelastEditedLoc();
}

typePage *newPage(INT base, size_t size) 
{ 
  typePage *p = (typePage *) malloc(sizeof(typePage));
  p->base = base;
  p->size = size;
  p->vals = malloc(size);
  return p;
}

void freePage(typePage *page)
{
  free(page->vals);
  free(page);
}
