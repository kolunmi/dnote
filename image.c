#include <png.h>
#include <stdio.h>
#include <stdlib.h>

#include "image.h"
#include "util.h"

void
pngimage_free(PngImage *img) {
    unsigned int i;
    
    for (i = 0; i < img->h; i++)
	free(img->rows[i]);
    free(img->rows);
    img->rows = NULL;
}

PngImage *
read_png_to_image(PngImage *img, char *path)
{
    unsigned int i, w, h;
    FILE *fp;
    png_byte color_type;
    png_byte bit_depth;
    png_structp png;
    png_infop info;

    if (img == NULL || path == NULL)
	return NULL;
    
    if ((fp = fopen(path, "rb")) == NULL)
	return NULL;

    if ((png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL)) == NULL)
	return NULL;

    if ((info = png_create_info_struct(png)) == NULL) {
	png_destroy_read_struct(&png, &info, NULL);
	return NULL;
    }

    if (setjmp(png_jmpbuf(png))) {
	png_destroy_read_struct(&png, &info, NULL);
	fclose(fp);
	return NULL;
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    w = png_get_image_width(png, info);
    h = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth = png_get_bit_depth(png, info);

    // Read any color_type into 8bit depth, RGBA format.
    // See http://www.libpng.org/pub/png/libpng-manual.txt

    if (bit_depth == 16)
	png_set_strip_16(png);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
	png_set_palette_to_rgb(png);

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
	png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
	png_set_tRNS_to_alpha(png);

    // These color_type don't have an alpha channel then fill it with 0xff.
    if (color_type == PNG_COLOR_TYPE_RGB ||
	color_type == PNG_COLOR_TYPE_GRAY ||
	color_type == PNG_COLOR_TYPE_PALETTE)
	png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if(color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
	png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    pngimage_free(img);
    
    img->w = w;
    img->h = h;
    
    img->rows = (png_bytep *) malloc(sizeof(png_bytep) * img->h);
    for(i = 0; i < img->h; i++)
	img->rows[i] = (png_byte *) malloc(png_get_rowbytes(png, info));

    png_read_image(png, img->rows);

    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);

    return img;
}
