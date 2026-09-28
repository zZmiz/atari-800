#ifndef FRUIT_JAM_UI_STYLE_H
#define FRUIT_JAM_UI_STYLE_H

#include <string.h>

/* Menu pixels only. Never change the emulated Atari palette or game pixels. */
static inline int fruit_jam_ui_colour(int colour)
{
    if (colour == 0x94) return 0x0f; /* white paper */
    if (colour == 0x9a) return 0x00; /* black text / selection background */
    return colour;
}

/* Browser filter, not a file decoder: AFILE still validates the file itself. */
static inline int fruit_jam_atari_image_name(const char *name)
{
    static const char *const extensions[] = {
        ".atr", ".atx", ".xfd", ".dcm", ".pro", ".xex", ".com", ".exe",
        ".car", ".rom", ".bin", ".cas", ".wav", ".bas", ".lst", ".a8s"
    };
    const char *ext = strrchr(name, '.');
    unsigned i;
    if (name[0] == '.' || ext == NULL) return 0;
    for (i = 0; i < sizeof extensions / sizeof extensions[0]; ++i) {
        const char *a = ext, *b = extensions[i];
        while (*a && *b) {
            char ch = *a++;
            if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
            if (ch != *b++) break;
            if (!*a && !*b) return 1;
        }
    }
    return 0;
}

#endif
