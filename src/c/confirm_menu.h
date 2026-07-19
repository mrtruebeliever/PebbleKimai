#pragma once
#include <pebble.h>

// A small confirmation menu that works with both the buttons and touch (unlike
// the system ActionMenu, which ignores touch). Used for the stop / switch /
// refresh confirmations.
typedef void (*ConfirmAction)(void);

typedef struct {
  const char   *label;
  ConfirmAction cb;      // run after the menu closes; NULL just dismisses
} ConfirmItem;

// Pushes the menu. `items` is copied (up to a small maximum); labels must point
// to storage that outlives the menu (i18n strings or static buffers are fine).
void confirm_menu_show(const ConfirmItem *items, int count);
