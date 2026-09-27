#ifndef EMOJI_H
#define EMOJI_H

#define EMOJI_HEART 0
#define EMOJI_LIKE  1
#define EMOJI_FUNNY 2
#define EMOJI_SAD   3

void emoji_draw(
    int type,
    int x,
    int y,
    int size
);

#endif
