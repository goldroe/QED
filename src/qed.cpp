#include "qed.h"
#include "buffer.h"
#include "draw.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <assert.h>
#include <stdio.h>
#include <string.h>

String buffer_to_string(Buffer *buffer) {
    int64 buffer_length = get_buffer_length(buffer);
    String result{};
    result.data = (char *)malloc(buffer_length);
    memcpy(result.data, buffer->text, buffer->gap_start);
    memcpy(result.data + buffer->gap_start, buffer->text + buffer->gap_end, buffer->size - buffer->gap_end);
    result.count = buffer_length;
    return result;
}

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
