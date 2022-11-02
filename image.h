#include <png.h>

typedef struct {
    unsigned int w, h;
    png_bytep *rows;
} PngImage;

void pngimage_free(PngImage *img);
PngImage *read_png_to_image(PngImage *img, char *path);
