#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "buffer.h"

/*
 * Read file to buffer
 */
static void read_file(buffer_t *buf, const char *filename) {
  // read file to memory
  FILE *fp = fopen(filename, "r");
  if (fp) {
    char *line = NULL;
    size_t len = 0;

    for (size_t i = 0; getline(&line, &len, fp) != -1; i++) {
      // trim newline char
      line[strcspn(line, "\n")] = 0;

      append_row(buf, line);
    }

    free(line);
    line = NULL;
    fclose(fp);
  }

  // insert empty row if file is empty or doesn't exist
  if (!buf->current) {
    append_row(buf, NULL);
  }
}

void add_char(row_t *r, char c) {
  echar_t *ec = malloc(sizeof(echar_t));
  echar_t *next = NULL;
  echar_t *prev = NULL;

  ec->c = c;
  ec->prev = NULL;
  ec->next = NULL;

  if (r->line_size == 0) {
    r->head = ec;
    r->last = ec;
    r->current = ec;
    r->line_size ++;
    r->is_dirty = true;
    return;
  }

  if (r->current == r->last) {
    prev = r->current;
    prev->next = ec;
    r->last = ec;
  } else {
    prev = r->current;
    next = r->current->next;
    prev->next = ec;
    next->prev = ec;
  }

  ec->next = next;
  ec->prev = prev;

  r->is_dirty = true;
  r->line_size ++;
  r->current = ec;
}

/*
 * Init a row with a NULL char at the beginning
 */
static row_t *init_row(const char *line) {
  row_t *r = malloc(sizeof(row_t));

  r->next = NULL;
  r->prev = NULL;
  r->line_size = 0;
  r->is_dirty = true;

  r->head = NULL;
  r->last = NULL;
  r->current = NULL;

  if (!line) {
    return r;
  }

  for (size_t i = 0; i < strlen(line); i ++) {
    add_char(r, line[i]);
  }

  // set current to be the first char
  r->current = r->head;
  return r;
}

buffer_t *init_buffer(const char *filename) {
  buffer_t *buf = malloc(sizeof(buffer_t));

  buf->num_rows = 0;
  buf->head = NULL;
  buf->last = NULL;
  buf->current = NULL;
  buf->status_row = init_row(NULL);

  buf->current_row = 0;
  buf->current_char = 0;

  buf->is_dirty = false;
  buf->filename = filename;

  read_file(buf, filename);

  // we want to start at the top when user opens up a new file
  buf->current = buf->head;
  // Set current to the first char of line
  // TODO set current to the first non space char
  buf->current->current = buf->current->head;

  return buf;
}

void append_char(buffer_t *buf, char c) {
  if (buf->current->line_size != 0) {
    buf->current_char ++;
  }

  add_char(buf->current, c);
}

void prepend_char(buffer_t *buf, char c) {
  row_t *r = buf->current;

  assert(r->line_size > 0);

  // insert at head
  if (r->head && r->current == r->head) {
    echar_t *ec = malloc(sizeof(echar_t));
    ec->c = c;
    ec->prev = NULL;
    ec->next = r->head;

    r->head->prev = ec;
    r->head = ec;
    r->current = ec;
    r->current = r->current->next;

    buf->current_char ++;
    r->is_dirty = true;
    r->line_size ++;
    return;
  }

  r->current = r->current->prev;
  append_char(buf, c);
  r->current = r->current->next;
}

void delete_char(buffer_t *buf) {
  row_t *r = buf->current;

  if (r->line_size == 0) {
    return;
  }

  echar_t *to_delete = r->current;

  if (r->line_size == 1) {
    to_delete = r->current;
    r->current = NULL;
    r->head = NULL;
    r->last = NULL;

    r->line_size --;
    r->is_dirty = true;

    free(to_delete);
    return;
  }

  if (to_delete == r->head) {
    r->current = to_delete->next;
    r->head = r->current;
    r->head->prev = NULL;
  } else if (to_delete == r->last) {
    r->current = to_delete->prev;
    r->last = r->current;
    r->last->next = NULL;
    buf->current_char --;
  } else {
    r->current = to_delete->next;
    r->current->prev = to_delete->prev;
    to_delete->prev->next = r->current;
  }

  r->line_size --;
  r->is_dirty = true;

  free(to_delete);
}

void append_row(buffer_t *buf, const char *line) {
  row_t *r = init_row(line);
  row_t *prev = NULL;
  row_t *next = NULL;

  if (buf->num_rows == 0) {
    buf->head = r;
    buf->last= r;
  } else if (buf->current == buf->last) {
    prev = buf->current;
    prev->next = r;
    buf->last = r;
  } else {
    prev = buf->current;
    next = buf->current->next;
    prev->next = r;
    next->prev = r;
  }

  r->next = next;
  r->prev = prev;

  buf->num_rows ++;
  buf->current = r;
}

void prepend_row(buffer_t *buf, const char *line) {
  // insert at head
  if (buf->head && buf->current == buf->head) {
    row_t *r = init_row(line);
    r->prev = NULL;
    r->next = buf->head;

    buf->head->prev = r;
    buf->head = r;
    buf->current = r;

    buf->num_rows ++;
    return;
  }

  buf->current = buf->current->prev;
  append_row(buf, line);
}

static void adjust_current_char(buffer_t *buf) {
  row_t *r = buf->current;
  echar_t *ec = r->head;

  if (!ec) {
    buf->current_char = 0;
    return;
  }

  for (size_t i = 0; i < buf->current_char; i ++) {
    if (!ec->next) {
      buf->current_char = i;
      break;
    }

    ec = ec->next;
  }

  r->current = ec;
}

static void move_up(buffer_t *buf) {
  if (buf->current->prev) {
    buf->current = buf->current->prev;
    buf->current_row --;
    adjust_current_char(buf);
  }
}

static void move_down(buffer_t *buf) {
  if (buf->current->next) {
    buf->current = buf->current->next;
    buf->current_row ++;
    adjust_current_char(buf);
  }
}

static void move_left(buffer_t *buf) {
  row_t *r = buf->current;
  if (r->current && r->current->prev) {
    r->current = r->current->prev;
    buf->current_char --;
  }
}

static void move_right(buffer_t *buf) {
  row_t *r = buf->current;
  if (r->current && r->current->next) {
    r->current = r->current->next;
    buf->current_char ++;
  }
}

void move_current(buffer_t *buf, DIRECTION d) {
  switch (d) {
    case UP:
      move_up(buf);
      break;
    case DOWN:
      move_down(buf);
      break;
    case LEFT:
      move_left(buf);
      break;
    case RIGHT:
      move_right(buf);
      break;
    default:
      break;
  }
}

void to_right(buffer_t *buf) {
  for (size_t i = buf->current_char; i < buf->current->line_size; i ++) {
    move_current(buf, RIGHT);
  }
}

void to_left(buffer_t *buf) {
  size_t cur = buf->current_char;
  for (size_t i = 0; i < cur; i ++) {
    move_current(buf, LEFT);
  }
}

void clear_row(row_t *r) {
  echar_t *ec = r->head;
  while (ec) {
    echar_t *tmp = ec;
    ec = ec->next;
    free(tmp);
  }

  r->last = r->head;
  r->current = r->head;
  r->line_size = 0;
}

static void destroy_row(row_t *r) {
  echar_t *c = r->head, *tmp;
  while (c) {
    tmp = c;
    c = c->next;
    free(tmp);
  }
  free(r);
}

void delete_row(buffer_t *buf) {
  row_t *to_delete = buf->current;

  // there is at least one row in buffer
  if (buf->num_rows == 1) {
    clear_row(buf->current);
    return;
  }

  if (to_delete == buf->head) {
    buf->current = to_delete->next;
    buf->current->prev = NULL;
    buf->head = buf->current;
  } else if (to_delete == buf->last) {
    buf->current = to_delete->prev;
    buf->current->next = NULL;
    buf->last = buf->current;
  } else {
    to_delete->prev->next = to_delete->next;
    to_delete->next->prev = to_delete->prev;
    buf->current = to_delete->next;
  }

  buf->num_rows --;
  destroy_row(to_delete);
}

void destroy_buffer(buffer_t *buf) {
  row_t *r = buf->head, *tmp;

  while (r) {
    tmp = r;
    r = r->next;
    destroy_row(tmp);
  }

  destroy_row(buf->status_row);
  free(buf);
}