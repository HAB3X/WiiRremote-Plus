#ifndef FONT_H
#define FONT_H
typedef struct { short adv, x0, y0, w, h; int off; } Glyph;
typedef struct { short height, ascent; const Glyph *g; const unsigned char *px; } Font;
extern const Font F_SMALL_font, F_BODY_font, F_BODYB_font, F_BTN_font, F_TITLE_font, F_BIG_font;
#define F_SMALL  (&F_SMALL_font)
#define F_BODY   (&F_BODY_font)
#define F_BODYB  (&F_BODYB_font)
#define F_BTN    (&F_BTN_font)
#define F_TITLE  (&F_TITLE_font)
#define F_BIG    (&F_BIG_font)
#endif
