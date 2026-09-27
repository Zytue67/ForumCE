#include <ti/getcsc.h>

#include "keyboard.h"

void keyboard_reset(KeyboardState *state)
{
    state->mode = KEYBOARD_NORMAL;
    state->secondPending = 0;
}

char keyboard_alpha_letter(sk_key_t key)
{
    if (key == sk_Math)    return 'A';
    if (key == sk_Apps)    return 'B';
    if (key == sk_Prgm)    return 'C';
    if (key == sk_Recip)   return 'D';
    if (key == sk_Sin)     return 'E';
    if (key == sk_Cos)     return 'F';
    if (key == sk_Tan)     return 'G';
    if (key == sk_Power)   return 'H';
    if (key == sk_Square)  return 'I';
    if (key == sk_Comma)   return 'J';
    if (key == sk_LParen)  return 'K';
    if (key == sk_RParen)  return 'L';
    if (key == sk_Div)     return 'M';
    if (key == sk_Log)     return 'N';
    if (key == sk_7)       return 'O';
    if (key == sk_8)       return 'P';
    if (key == sk_9)       return 'Q';
    if (key == sk_Mul)     return 'R';
    if (key == sk_Ln)      return 'S';
    if (key == sk_4)       return 'T';
    if (key == sk_5)       return 'U';
    if (key == sk_6)       return 'V';
    if (key == sk_Sub)     return 'W';
    if (key == sk_Store)   return 'X';
    if (key == sk_1)       return 'Y';
    if (key == sk_2)       return 'Z';

    /*
     * Basic characters available through
     * the calculator's alpha-style input.
     */
    if (key == sk_0)       return ' ';
    if (key == sk_DecPnt)  return ':';
    if (key == sk_Add)     return '"';

    /*
     * Question mark.
     *
     * This gives ? its own character in
     * Alpha mode without changing the
     * existing keyboard behavior.
     */
    if (key == sk_Chs)     return '?';

    return 0;
}

static char number_character(sk_key_t key)
{
    if (key == sk_0) return '0';
    if (key == sk_1) return '1';
    if (key == sk_2) return '2';
    if (key == sk_3) return '3';
    if (key == sk_4) return '4';
    if (key == sk_5) return '5';
    if (key == sk_6) return '6';
    if (key == sk_7) return '7';
    if (key == sk_8) return '8';
    if (key == sk_9) return '9';
    if (key == sk_DecPnt) return '.';

    return 0;
}

int keyboard_handle_key(
    KeyboardState *state,
    sk_key_t key,
    char *out
)
{
    char ch;

    *out = 0;

    /*
     * 2nd is only used here as the modifier for
     * 2nd + ALPHA.
     */
    if (key == sk_2nd)
    {
        state->secondPending = 1;
        return 0;
    }

    /*
     * Handle 2nd + ALPHA.
     *
     * NORMAL -> UPPER
     * UPPER  -> NORMAL
     * LOWER  -> NORMAL
     */
    if (state->secondPending)
    {
        state->secondPending = 0;

        if (key == sk_Alpha)
        {
            if (state->mode == KEYBOARD_NORMAL)
                state->mode = KEYBOARD_UPPER;
            else
                state->mode = KEYBOARD_NORMAL;

            return 0;
        }

        return 0;
    }

    /*
     * ALPHA cycles:
     *
     * NORMAL -> UPPER
     * UPPER  -> LOWER
     * LOWER  -> NORMAL
     */
    if (key == sk_Alpha)
    {
        if (state->mode == KEYBOARD_NORMAL)
            state->mode = KEYBOARD_UPPER;
        else if (state->mode == KEYBOARD_UPPER)
            state->mode = KEYBOARD_LOWER;
        else
            state->mode = KEYBOARD_NORMAL;

        return 0;
    }

    /*
     * Normal mode:
     * ONLY numbers are accepted.
     */
    if (state->mode == KEYBOARD_NORMAL)
    {
        ch = number_character(key);

        if (ch)
        {
            *out = ch;
            return 1;
        }

        return 0;
    }

    /*
     * Alpha modes.
     */
    ch = keyboard_alpha_letter(key);

    if (ch)
    {
        if (state->mode == KEYBOARD_LOWER)
        {
            if (ch >= 'A' && ch <= 'Z')
                ch = (char)(ch + ('a' - 'A'));
        }

        *out = ch;
        return 1;
    }

    return 0;
}

const char *keyboard_mode_name(
    const KeyboardState *state
)
{
    if (state->mode == KEYBOARD_UPPER)
        return "ABC";

    if (state->mode == KEYBOARD_LOWER)
        return "abc";

    return "123";
}
