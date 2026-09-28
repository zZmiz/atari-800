#include "FrensFonts.h"
#include "font_8x8.h"

char getcharslicefrom8x8font(char c, int rowInChar)
{
    if ((unsigned char)c < FONT_FIRST_ASCII ||
        (unsigned char)c >= FONT_FIRST_ASCII + FONT_N_CHARS ||
        rowInChar < 0 || rowInChar >= FONT_CHAR_HEIGHT)
        return 0;
    return font_8x8[(c - FONT_FIRST_ASCII) + (rowInChar)*FONT_N_CHARS];
}
