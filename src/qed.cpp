#include "qed.h"
#include "buffer.h"
#include "draw.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

Face *load_font_face(const char *font_name, int font_height) {
    FT_Library ft_lib;
    int err = FT_Init_FreeType(&ft_lib);
    if (err) {
        printf("Error initializing freetype library: %d\n", err);
    }

    FT_Face ft_face;
    err = FT_New_Face(ft_lib, font_name, 0, &ft_face);
    if (err == FT_Err_Unknown_File_Format) {
        printf("Format not supported\n");
    } else if (err) {
        printf("Font file could not be read\n");
    }

    err = FT_Set_Pixel_Sizes(ft_face, 0, font_height);
    if (err) {
        printf("Error setting pixel sizes of font\n");
    }

    Face *face = new Face();

    int bbox_ymax = FT_MulFix(ft_face->bbox.yMax, ft_face->size->metrics.y_scale) >> 6;
    int bbox_ymin = FT_MulFix(ft_face->bbox.yMin, ft_face->size->metrics.y_scale) >> 6;
    int height = bbox_ymax - bbox_ymin;
    float ascend = ft_face->size->metrics.ascender / 64.f;
    float descend = ft_face->size->metrics.descender / 64.f;
    float bbox_height = (float)(bbox_ymax - bbox_ymin);
    float glyph_height = (float)ft_face->size->metrics.height / 64.f;
    float glyph_width = (float)(ft_face->size->metrics.max_advance) / 64.f;

    int atlas_width = 1; // white pixel
    int atlas_height = 0;
    int max_bmp_height = 0;
    for (uint32 c = 0; c < 256; c++) {
        if (FT_Load_Char(ft_face, c, FT_LOAD_RENDER)) {
            printf("Error loading char %c\n", c);
            continue;
        }

        atlas_width += ft_face->glyph->bitmap.width;
        if (atlas_height < (int)ft_face->glyph->bitmap.rows) {
            atlas_height = ft_face->glyph->bitmap.rows;
        }

        int bmp_height = ft_face->glyph->bitmap.rows + ft_face->glyph->bitmap_top;
        if (max_bmp_height < bmp_height) {
            max_bmp_height = bmp_height;
        }
    }

    int atlas_x = 1; // white pixel


    // Pack glyph bitmaps
    unsigned char *bitmap = (unsigned char *)calloc(atlas_width * atlas_height + 1, 1);
    bitmap[0] = 255;
    for (uint32 c = 0; c < 256; c++) {
        if (FT_Load_Char(ft_face, c, FT_LOAD_RENDER)) {
            printf("Error loading char '%c'\n", c);
        }

        Glyph *glyph = &face->glyphs[c];
        glyph->ax = (float)(ft_face->glyph->advance.x >> 6);
        glyph->ay = (float)(ft_face->glyph->advance.y >> 6);
        glyph->bx = (float)ft_face->glyph->bitmap.width;
        glyph->by = (float)ft_face->glyph->bitmap.rows;
        glyph->bt = (float)ft_face->glyph->bitmap_top;
        glyph->bl = (float)ft_face->glyph->bitmap_left;
        glyph->to = (float)atlas_x / atlas_width;
            
        // Write glyph bitmap to atlas
        for (int y = 0; y < glyph->by; y++) {
            unsigned char *dest = bitmap + y * atlas_width + atlas_x;
            unsigned char *source = ft_face->glyph->bitmap.buffer + y * ft_face->glyph->bitmap.width;
            memcpy(dest, source, ft_face->glyph->bitmap.width);
        }

        atlas_x += ft_face->glyph->bitmap.width;
    }

    void *d3d11_create_face_texture(uint8 *bitmap, int width, int height);
    face->width = atlas_width;
    face->height = atlas_height;
    face->max_bmp_height = max_bmp_height;
    face->ascend = ascend;
    face->descend = descend;
    face->bbox_height = height;
    face->glyph_width = glyph_width;
    face->glyph_height = glyph_height;
    face->texture = d3d11_create_face_texture(bitmap, atlas_width, atlas_height);

    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_lib);
    return face;
}

uint32 hex_to_uint32(char *stream, char **ptr) {
    uint32 result = 0;
    while (isalnum(*stream)) {
        int digit = 0;
        char c = toupper(*stream);
        if (c >= 'A' && c <= 'Z') {
            digit = c - 'A' + 10; 
        } else if (c >= '0' && c <= '9') {
            digit = c - '0';
        }
        result = result * 16 + digit;

        stream++;
    }
    if (ptr) *ptr = stream;
    return result;
}

// This is really lazy
// @Speed make a hash table for the theme field names
Theme_Color theme_name_to_color(char *name) {
    if (strcmp(name, "default") == 0) {
        return THEME_COLOR_DEFAULT;
    } else if (strcmp(name, "background") == 0) {
        return THEME_COLOR_BACKGROUND; 
    } else if (strcmp(name, "region") == 0) {
        return THEME_COLOR_REGION;
    } else if (strcmp(name, "cursor") == 0) {
        return THEME_COLOR_CURSOR;
    } else if (strcmp(name, "cursor_char") == 0) {
        return THEME_COLOR_CURSOR_CHAR;
    } else if (strcmp(name, "ui_default") == 0) {
        return THEME_COLOR_UI_DEFAULT;
    } else if (strcmp(name, "ui_background") == 0) {
        return THEME_COLOR_UI_BACKGROUND;
    } else {
        assert(0);
        return THEME_COLOR_NONE;
    }
}

void theme_set_field(Theme *theme, char *name, uint32 color_value) {
    Theme_Color color = theme_name_to_color(name);
    theme->colors[color] = color_value;
}

//@Todo Make more robust
// This is a very lazy parser
Theme *load_theme(const char *file_name) {
    Theme *theme = new Theme();

    String string = read_file_string(file_name);
    char *stream = string.data;

    for (;;) {
        if (*stream == 0) break;
        
        switch (*stream) {
        default:
        {
            char *start = stream;
            while (*stream != ':') {
                stream++;
            }
            assert(*stream == ':');

            int64 field_len = stream - start;
            char *field_name = (char *)malloc(field_len + 1);
            strncpy(field_name, start, field_len);
            field_name[field_len] = 0;

            stream++;
            while (isspace(*stream)) {
                stream++;
            }

            assert(isalnum(*stream));

            uint32 color_value = hex_to_uint32(stream, &stream);
            theme_set_field(theme, field_name, color_value);
            break;
        }
        case '#':
            while (*stream != 0 &&
                *stream != '\n' && *stream != '\r') {
                stream++;
            }
            break;
        case '[':
        {
            stream++;
            int version = strtol(stream, &stream, 10);
            assert(*stream == ']');
            stream++;
            break;
        }
        case '\n': case ' ': case '\f': case '\t': case '\r':
            while (isspace(*stream)) {
                stream++;
            }
            break;
        }
    }

    return theme;
}
