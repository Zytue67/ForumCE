#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <ti/getcsc.h>

typedef enum
{
    KEYBOARD_NORMAL = 0,
    KEYBOARD_UPPER,
    KEYBOARD_LOWER
} KeyboardMode;

typedef struct
{
    KeyboardMode mode;
    int secondPending;
} KeyboardState;

void keyboard_reset(KeyboardState *state);

char keyboard_alpha_letter(sk_key_t key);

int keyboard_handle_key(
    KeyboardState *state,
    sk_key_t key,
    char *out
);

const char *keyboard_mode_name(
    const KeyboardState *state
);

#endif
