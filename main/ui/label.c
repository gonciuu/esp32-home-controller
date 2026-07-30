#include "label.h"
#include "theme.h"

#include <string.h>

lv_obj_t *ui_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, ui_color(color), 0);
    return label;
}

void ui_label_set(lv_obj_t *label, char *cache, size_t cap, const char *text)
{
    if (strcmp(cache, text) == 0) {
        return;
    }
    strlcpy(cache, text, cap);
    lv_label_set_text(label, text);
}
