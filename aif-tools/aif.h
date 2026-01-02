#ifndef AIF_H
#define AIF_H

#include <stdint.h>

#define AIF_MAGIC "AIF"
#define AIF_MAGIC_SIZE (sizeof(AIF_MAGIC))

#define AIF_CHECKSUM_OFFSET AIF_MAGIC_SIZE
#define AIF_CHECKSUM_SIZE 2
#define AIF_PXL_FMT_OFFSET (AIF_CHECKSUM_OFFSET + AIF_CHECKSUM_SIZE)
#define AIF_PXL_FMT_SIZE 1
#define AIF_COMPRESSION_OFFSET (AIF_PXL_FMT_OFFSET + AIF_PXL_FMT_SIZE)
#define AIF_COMPRESSION_SIZE 1
#define AIF_WIDTH_OFFSET (AIF_COMPRESSION_OFFSET + AIF_COMPRESSION_SIZE)
#define AIF_WIDTH_SIZE 4
#define AIF_HEIGHT_OFFSET (AIF_WIDTH_OFFSET + AIF_WIDTH_SIZE)
#define AIF_HEIGHT_SIZE 4
#define AIF_PXL_OFFSET_OFFSET (AIF_HEIGHT_OFFSET + AIF_HEIGHT_SIZE)
#define AIF_PXL_OFFSET_SIZE 4

#define AIF_HEADER_SIZE (AIF_PXL_OFFSET_OFFSET + AIF_PXL_OFFSET_SIZE)

#define AIF_FMT_RGB8 (1)
#define AIF_FMT_GRAY8 (2)

#define AIF_COMPRESSION_NONE (0)
#define AIF_COMPRESSION_RLE (1)

// Takes in a pixel format and returns the number of bits per pixel
int aif_pixel_format_bpp(int format);
// Takes in a pixel format and returns its name as a string
const char *aif_pixel_format_name(int format);
// Takes in a compression format and returns its name as a string
const char *aif_compression_name(int compression);

void stage1_info(int n_files, const char **files);
void stage2_brighten(int amount, const char *in_file, const char *out_file);
void stage3_convert_color(const char *color, const char *in_file, const char *out_file);
void stage4_decompress(const char *in_file, const char *out_file);
void stage5_compress(const char *in_file, const char *out_file);

#endif
