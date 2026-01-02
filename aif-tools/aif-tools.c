// Description: This is program that can manipulate AIF (Amazing Image Format)
//              files. It can display information about the file, brighten
//              the pixels, convert between colour formats, decompress and 
//              compress the pixels.
//
// Name: Ethan Tong
// zID: z5691989
// Date Completed: 21/11/2025

#include "aif.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#define FALSE 0
#define TRUE 1

// Takes in a RGB color and brightens it by the given percentage amount
uint32_t brighten_rgb(uint32_t color, int amount);
uint16_t read_le_u16(const uint8_t *buf);
uint32_t read_le_u32(const uint8_t *buf);
int aif_magic_valid(const uint8_t *h);
int aif_format_valid(uint8_t fmt);
int aif_dim_valid(uint32_t n);
uint16_t compute_checksum(FILE *f, int file_size);
void print_with_invalid_flag(const char *label, uint32_t value, int valid);
FILE *aif_open_and_read_header(
    const char *filename,
    uint8_t header[AIF_HEADER_SIZE],
    int *file_size
);
uint8_t *aif_decompress_image(FILE *fp, uint32_t width, uint32_t height, size_t bpp);
size_t compress_row(
    const uint8_t *row,
    uint32_t width,
    size_t bpp,
    uint8_t *out
);
// Description: Compress an entire image row-by-row and write to file.
// Params:
// - out: output FILE*
// - pixels: raw pixel buffer
// - width: image width in pixels
// - height: image height in pixels
// - bpp: bytes per pixel
// Returns: total bytes of compressed image data written.
size_t aif_write_compressed_rows(
    FILE *out,
    const uint8_t *pixels,
    uint32_t width,
    uint32_t height,
    size_t bpp
);

// Description: Stage 1 entry; print header info and validation for each AIF file.
// Params:
// - n_files: number of input files
// - files: array of filenames
// Returns: void; exits on error.
void stage1_info(int n_files, const char **files) {
    for (int i = 0; i < n_files; i++) {

        const char *filename = files[i];

        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
            fprintf(stderr, "Failed to open file: No such file or directory\n");
            exit(1);
        }

        struct stat st;
        stat(filename, &st);
        int file_size = st.st_size;

        uint8_t header[AIF_HEADER_SIZE];
        size_t n = fread(header, 1, AIF_HEADER_SIZE, file);
        if (n < AIF_HEADER_SIZE) {
            fprintf(stderr, "Unexpected EOF\n");
            exit(1);
        }


        uint16_t stored_checksum = read_le_u16(&header[AIF_CHECKSUM_OFFSET]);
        uint8_t pixel_format = header[AIF_PXL_FMT_OFFSET];
        uint8_t compression = header[AIF_COMPRESSION_OFFSET];
        uint32_t width = read_le_u32(&header[AIF_WIDTH_OFFSET]);
        uint32_t height = read_le_u32(&header[AIF_HEIGHT_OFFSET]);


        int magic_ok = aif_magic_valid(header);
        int format_ok = aif_format_valid(pixel_format);
        int width_ok = aif_dim_valid(width);
        int height_ok = aif_dim_valid(height);

        // Checksum
        uint16_t calc_checksum = compute_checksum(file, file_size);
        int checksum_ok = (calc_checksum == stored_checksum);

        printf("<%s>:\n", filename);
        printf("File-size: %d bytes\n", file_size);
        if (!magic_ok) {
            printf("Invalid header magic.\n");
        }
        // Checksum output
        printf("Checksum: %02x %02x",
               (stored_checksum >> 8) & 0xFF,
               stored_checksum & 0xFF);
        if (!checksum_ok)
            printf(" INVALID, calculated %02x %02x",
                   (calc_checksum >> 8) & 0xFF,
                   calc_checksum & 0xFF);
        printf("\n");

        // Pixel format
        if (format_ok)
            printf("Pixel format: %s\n", aif_pixel_format_name(pixel_format));
        else
            printf("Pixel format: Invalid\n");

        // Compression (always valid in Stage 1 printing)
        printf("Compression: %s\n", aif_compression_name(compression));

        // Width / Height
        print_with_invalid_flag("Width",  width,  width_ok);
        print_with_invalid_flag("Height", height, height_ok);

        fclose(file);
    }
}


// Description: Open an AIF file, read its header, and report file size.
// Params:
// - filename: path to input AIF
// - header: output buffer for header bytes
// - file_size: output size of the file
// Returns: FILE* positioned after header; exits on error.
FILE *aif_open_and_read_header(
    const char *filename,
    uint8_t header[AIF_HEADER_SIZE],
    int *file_size
) {
    // Open file
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open file: No such file or directory\n");
        exit(EXIT_FAILURE);
    }

    // Stat to get size
    struct stat st;
    stat(filename, &st);
    *file_size = st.st_size;

    // Read header
    size_t n = fread(header, 1, AIF_HEADER_SIZE, file);
    if (n < AIF_HEADER_SIZE) {
        fprintf(stderr, "Unexpected EOF\n");
        exit(EXIT_FAILURE);
    }

    return file;
}

// Description: Read a 16-bit little-endian unsigned integer.
// Params:
// - buf: pointer to at least 2 bytes
// Returns: uint16_t value read.
uint16_t read_le_u16(const uint8_t *buf) {
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

// Description: Read a 32-bit little-endian unsigned integer.
// Params:
// - buf: pointer to at least 4 bytes
// Returns: uint32_t value read.
uint32_t read_le_u32(const uint8_t *buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

// Description: Check whether header bytes match the AIF magic.
// Params:
// - h: pointer to start of header
// Returns: TRUE if valid, otherwise FALSE.
int aif_magic_valid(const uint8_t *h) {
    return h[0] == 0x41 && h[1] == 0x49 && h[2] == 0x46 && h[3] == 0x00;
}

// Description: Validate pixel format byte.
// Params:
// - format: pixel format code
// Returns: TRUE if recognised, otherwise FALSE.
int aif_format_valid(uint8_t format) {
    return format == AIF_FMT_RGB8 || format == AIF_FMT_GRAY8;
}

// Description: Validate dimension field.
// Params:
// - n: dimension value
// Returns: TRUE if > 0, otherwise FALSE.
int aif_dim_valid(uint32_t n) {
    return n > 0;
}

// Description: Compute AIF checksum with checksum bytes zeroed.
// Params:
// - f: open file handle
// - file_size: total file size in bytes
// Returns: checksum value.
uint16_t compute_checksum(FILE *f, int file_size) {
    fseek(f, 0, SEEK_SET);

    int sum1 = 0;
    int sum2 = 0;

    for (int pos = 0; pos < file_size; pos++) {
        int byte = fgetc(f);
        if (byte == EOF) {
            fprintf(stderr, "Unexpected EOF\n");
            exit(1);
        }

        // Treat stored checksum bytes as 0
        if (pos == AIF_CHECKSUM_OFFSET || pos == AIF_CHECKSUM_OFFSET + 1) {
            byte = 0;
        }

        sum1 = (sum1 + byte) % 256;
        sum2 = (sum2 + sum1) % 256;
    }

    return (uint16_t)((sum2 << 8) | sum1);
}

// Description: Print a labelled dimension with optional INVALID suffix.
// Params:
// - label: text for field
// - value: dimension value
// - valid: validity flag
// Returns: void.
void print_with_invalid_flag(const char *label, uint32_t value, int valid) {
    printf("%s: %u px", label, value);
    if (!valid) printf(" INVALID");
    printf("\n");
}

// Description: Stage 2; brighten an image by a percentage.
// Params:
// - amount: brighten/darken percentage (-100..100)
// - in_file: input AIF path
// - out_file: output AIF path
// Returns: void; exits on error.
void stage2_brighten(int amount, const char *in_file, const char *out_file) {
    uint8_t header[AIF_HEADER_SIZE];
    int file_size;
    FILE *in = aif_open_and_read_header(in_file, header, &file_size);

    uint8_t pixel_format = header[AIF_PXL_FMT_OFFSET];
    uint8_t compression = header[AIF_COMPRESSION_OFFSET];
    uint32_t width = read_le_u32(&header[AIF_WIDTH_OFFSET]);
    uint32_t height = read_le_u32(&header[AIF_HEIGHT_OFFSET]);

    // Validate header fields
    if (!aif_magic_valid(header)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }

    if (!aif_format_valid(pixel_format)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }

    if (!aif_dim_valid(width) || !aif_dim_valid(height)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }

    if (compression != AIF_COMPRESSION_NONE && compression != AIF_COMPRESSION_RLE) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }

    size_t bpp;
    if (pixel_format == AIF_FMT_RGB8) {
        bpp = 3;
    } else {
        bpp = 1;
    }

    size_t pixel_bytes = bpp * (size_t)width * (size_t)height;

    // Load pixels, expanding compressed input if needed
    uint8_t *pixel_data = NULL;
    if (compression == AIF_COMPRESSION_NONE) {
        pixel_data = malloc(pixel_bytes);

        size_t n = fread(pixel_data, 1, pixel_bytes, in);
        if (n < pixel_bytes) {
            fprintf(stderr, "Unexpected EOF\n");
            exit(EXIT_FAILURE);
        }

        fclose(in);
    } else {
        pixel_data = aif_decompress_image(in, width, height, bpp);
        fclose(in);
    }

    // Apply brighten to raw pixels
    if (pixel_format == AIF_FMT_GRAY8) { 
        for (size_t i = 0; i < pixel_bytes; i++) {
            int16_t val = pixel_data[i];
            val = val + (val * amount / 100);
            if (val > 255) val = 255;
            if (val < 0) val = 0;
            pixel_data[i] = (uint8_t)val;
        }
    } else if (pixel_format == AIF_FMT_RGB8) {
        for (size_t i = 0; i < pixel_bytes; i += 3) {
            uint32_t colour = (pixel_data[i] << 16)
                           | (pixel_data[i + 1] << 8)
                           | (pixel_data[i + 2]);
            colour = brighten_rgb(colour, amount);
            pixel_data[i]     = (colour >> 16) & 0xFF;
            pixel_data[i + 1] = (colour >> 8) & 0xFF;
            pixel_data[i + 2] = colour & 0xFF;
        }
    }

    uint8_t output_compression = compression;
    header[AIF_COMPRESSION_OFFSET] = output_compression;

    // Write output file
    FILE *out = fopen(out_file, "wb");
    if (out == NULL) {
        fprintf(stderr, "Failed to open output file: No such file or directory\n");
        exit(EXIT_FAILURE);
    }

    // Write header
    fwrite(header, 1, AIF_HEADER_SIZE, out);

    size_t data_bytes = 0;
    if (output_compression == AIF_COMPRESSION_NONE) {
        fwrite(pixel_data, 1, pixel_bytes, out);
        data_bytes = pixel_bytes;
    } else {
        data_bytes = aif_write_compressed_rows(out, pixel_data, width, height, bpp);
    }

    free(pixel_data);

    fflush(out);
    fclose(out);

    // Reopen for updating in read+write mode
    FILE *out_rw = fopen(out_file, "r+b");
    if (out_rw == NULL) {
        fprintf(stderr, "Failed to open output file: No such file or directory\n");
        exit(EXIT_FAILURE);
    }

    // Compute checksum
    int out_file_size = AIF_HEADER_SIZE + (int)data_bytes;
    uint16_t new_checksum = compute_checksum(out_rw, out_file_size);

    // Write checksum back into header
    header[AIF_CHECKSUM_OFFSET]     = new_checksum & 0xFF;
    header[AIF_CHECKSUM_OFFSET + 1] = (new_checksum >> 8) & 0xFF;

    // Rewrite header at beginning
    fseek(out_rw, 0, SEEK_SET);
    fwrite(header, 1, AIF_HEADER_SIZE, out_rw);

    fclose(out_rw);
}


// Description: Stage 3; convert between gray8 and rgb8 formats (preserving compression).
// Params:
// - color: target format string ("gray8" or "rgb8")
// - in_file: input AIF path
// - out_file: output AIF path
// Returns: void; exits on error.
void stage3_convert_color(const char *color, const char *in_file, const char *out_file) {

    uint8_t header[AIF_HEADER_SIZE];
    int file_size;
    FILE *in = aif_open_and_read_header(in_file, header, &file_size);

    uint8_t pixel_format = header[AIF_PXL_FMT_OFFSET];
    uint8_t compression  = header[AIF_COMPRESSION_OFFSET];
    uint32_t width       = read_le_u32(&header[AIF_WIDTH_OFFSET]);
    uint32_t height      = read_le_u32(&header[AIF_HEIGHT_OFFSET]);

    if (!aif_magic_valid(header)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }
    if (!aif_format_valid(pixel_format)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }
    if (compression != AIF_COMPRESSION_NONE && compression != AIF_COMPRESSION_RLE) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }
    if (!aif_dim_valid(width) || !aif_dim_valid(height)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }

    int target_fmt;
    if (strcmp(color, "gray8") == 0) {
        target_fmt = AIF_FMT_GRAY8;
    } else {
        target_fmt = AIF_FMT_RGB8;
    }

    size_t in_bpp;
    if (pixel_format == AIF_FMT_RGB8) {
        in_bpp = 3;
    } else {
        in_bpp = 1;
    }

    size_t in_size = in_bpp * (size_t)width * (size_t)height;

    // Load pixels, expanding compressed input if needed
    uint8_t *pixel_data = NULL;
    if (compression == AIF_COMPRESSION_NONE) {
        pixel_data = malloc(in_size);

        if (fread(pixel_data, 1, in_size, in) < in_size) {
            fprintf(stderr, "Unexpected EOF\n");
            exit(EXIT_FAILURE);
        }
        fclose(in);
    } else {
        pixel_data = aif_decompress_image(in, width, height, in_bpp);
        fclose(in);
    }

    // Default to reusing the buffer unless we change format
    uint8_t *out_pixels = pixel_data;
    size_t out_bpp = in_bpp;

    // RGB -> GRAY
    if (pixel_format == AIF_FMT_RGB8 && target_fmt == AIF_FMT_GRAY8) {
        out_bpp = 1;
        size_t out_size = (size_t)width * (size_t)height;
        out_pixels = malloc(out_size);

        for (size_t i = 0, j = 0; j < out_size; i += 3, j++) {
            uint8_t r = pixel_data[i];
            uint8_t g = pixel_data[i + 1];
            uint8_t b = pixel_data[i + 2];
            out_pixels[j] = (uint8_t)((r * 299 + g * 587 + b * 114) / 1000);
        }

        free(pixel_data);
        header[AIF_PXL_FMT_OFFSET] = AIF_FMT_GRAY8;

    // GRAY -> RGB
    } else if (pixel_format == AIF_FMT_GRAY8 && target_fmt == AIF_FMT_RGB8) {
        out_bpp = 3;
        size_t out_size = 3 * (size_t)width * (size_t)height;
        out_pixels = malloc(out_size);

        for (size_t i = 0, j = 0; i < in_size; i++, j += 3) {
            uint8_t g = pixel_data[i];
            out_pixels[j]     = g;
            out_pixels[j + 1] = g;
            out_pixels[j + 2] = g;
        }

        free(pixel_data);
        header[AIF_PXL_FMT_OFFSET] = AIF_FMT_RGB8;

    }

    // If we didn't change the format, make sure header still matches
    if (pixel_format == target_fmt) {
        header[AIF_PXL_FMT_OFFSET] = pixel_format;
    }

    uint8_t output_compression = compression;
    header[AIF_COMPRESSION_OFFSET] = output_compression;

    size_t out_size = out_bpp * (size_t)width * (size_t)height;

    FILE *out = fopen(out_file, "wb");
    if (out == NULL) {
        fprintf(stderr, "Failed to open output file: No such file or directory\n");
        exit(EXIT_FAILURE);
    }

    fwrite(header, 1, AIF_HEADER_SIZE, out);

    size_t data_bytes = 0;
    if (output_compression == AIF_COMPRESSION_NONE) {
        fwrite(out_pixels, 1, out_size, out);
        data_bytes = out_size;
    } else {
        data_bytes = aif_write_compressed_rows(out, out_pixels, 
                                               width, height, out_bpp);
    }

    free(out_pixels);
    fflush(out);
    fclose(out);

    FILE *rew = fopen(out_file, "r+b");
    if (rew == NULL) {
        fprintf(stderr, "Failed to open output file: No such file or directory\n");
        exit(EXIT_FAILURE);
    }

    int out_file_size = AIF_HEADER_SIZE + (int)data_bytes;
    uint16_t checksum = compute_checksum(rew, out_file_size);

    header[AIF_CHECKSUM_OFFSET]     = checksum & 0xFF;
    header[AIF_CHECKSUM_OFFSET + 1] = (checksum >> 8) & 0xFF;

    fseek(rew, 0, SEEK_SET);
    fwrite(header, 1, AIF_HEADER_SIZE, rew);
    fclose(rew);
}

// Description: Compare two pixels of size bpp for equality.
// Params:
// - a: pixel pointer
// - b: pixel pointer
// - bpp: bytes per pixel
// Returns: TRUE if identical, else FALSE.
int pixels_equal(const uint8_t *a, const uint8_t *b, size_t bpp) {
    for (size_t k = 0; k < bpp; k++) {
        if (a[k] != b[k]) {
            return FALSE;
        }
    }
    return TRUE;
}

// Description: Measure length of identical run starting at `start`.
// Params:
// - row: row data buffer
// - width: pixels in the row
// - bpp: bytes per pixel
// - start: column index to begin
// Returns: run length (>=1).
size_t measure_run(
    const uint8_t *row, 
    uint32_t width, 
    size_t bpp, 
    uint32_t start
) {
    size_t run = 1;
    while ((uint32_t)(start + run) < width) {
        const uint8_t *p1 = row + ((size_t)start * bpp);
        const uint8_t *p2 = row + ((size_t)(start + run) * bpp);
        if (!pixels_equal(p1, p2, bpp)) {
            break;
        }
        run++;
    }
    return run;
}

// Description: Emit one or more repeat blocks for a run of identical pixels.
// Params:
// - pixel: pixel bytes
// - run: run length in pixels
// - bpp: bytes per pixel
// - out: output buffer
// - out_pos: output cursor
// Returns: void.
void write_repeat_blocks(
    const uint8_t *pixel, 
    size_t run, 
    size_t bpp, 
    uint8_t *out,
    size_t *out_pos
) {
    size_t remaining = run;
    while (remaining > 0) {
        uint8_t chunk;
        if (remaining > 255) {
            chunk = 255;
        } else {
            chunk = (uint8_t)remaining;
        }

        out[*out_pos] = chunk;
        *out_pos = *out_pos + 1;

        for (size_t k = 0; k < bpp; k++) {
            out[*out_pos] = pixel[k];
            *out_pos = *out_pos + 1;
        }

        remaining = remaining - chunk;
    }
}

// Description: Emit literal blocks for a sequence of non-repeating pixels.
// Params:
// - row: row data buffer
// - start: starting column
// - literal_pixels: number of pixels to emit
// - bpp: bytes per pixel
// - out: output buffer
// - out_pos: output cursor
// Returns: void.
void write_literal_blocks(
    const uint8_t *row,
    uint32_t start,
    size_t literal_pixels,
    size_t bpp,
    uint8_t *out,
    size_t *out_pos
) {
    size_t emitted = 0;
    while (emitted < literal_pixels) {
        uint8_t chunk;
        if (literal_pixels - emitted > 255) {
            chunk = 255;
        } else {
            chunk = (uint8_t)(literal_pixels - emitted);
        }

        out[*out_pos] = 0;
        *out_pos = *out_pos + 1;
        out[*out_pos] = chunk;
        *out_pos = *out_pos + 1;

        size_t base = ((size_t)start + emitted) * bpp;
        for (uint8_t p = 0; p < chunk; p++) {
            for (size_t k = 0; k < bpp; k++) {
                out[*out_pos] = row[base + (size_t)p * bpp + k];
                *out_pos = *out_pos + 1;
            }
        }

        emitted = emitted + chunk;
    }
}

// Description: Compress a single row into the provided buffer.
// Params:
// - row: row data buffer
// - width: number of pixels in row
// - bpp: bytes per pixel
// - out: output buffer
// Returns: number of bytes written to out.
size_t compress_row(
    const uint8_t *row, 
    uint32_t width, 
    size_t bpp, 
    uint8_t *out
) {
    size_t out_pos = 0;
    uint32_t col = 0;

    while (col < width) {
        size_t run = measure_run(row, width, bpp, col);

        if (run >= 2) {
            const uint8_t *pixel = row + ((size_t)col * bpp);
            write_repeat_blocks(pixel, run, bpp, out, &out_pos);
            col += run;
            continue;
        }

        // No run: gather literals until the next run or row end
        uint32_t lit_start = col;
        col += 1;
        while (col < width) {
            size_t next_run = measure_run(row, width, bpp, col);
            if (next_run >= 2) {
                break;
            }
            col += 1;
        }

        size_t literal_pixels = (size_t)col - (size_t)lit_start;
        write_literal_blocks(row, lit_start, literal_pixels, 
                             bpp, out, &out_pos);
    }

    return out_pos;
}

// Description: Compress an entire image row-by-row and write to file.
// Params:
// - out: output FILE*
// - pixels: raw pixel buffer
// - width: image width in pixels
// - height: image height in pixels
// - bpp: bytes per pixel
// Returns: total bytes of compressed image data written.
size_t aif_write_compressed_rows(
    FILE *out,
    const uint8_t *pixels,
    uint32_t width,
    uint32_t height,
    size_t bpp
) {
    size_t row_bytes = (size_t)width * bpp;
    size_t max_row = (size_t)width * (bpp + 2);

    // Allocate once for worst-case literal expansion
    uint8_t *buffer = malloc(max_row);

    size_t total = 0;

    for (uint32_t r = 0; r < height; r++) {
        const uint8_t *row = pixels + (size_t)r * row_bytes;
        size_t comp_len = compress_row(row, width, bpp, buffer);

        uint8_t len_bytes[2] = {
            (uint8_t)(comp_len & 0xFF),
            (uint8_t)((comp_len >> 8) & 0xFF)
        };

        if (fwrite(len_bytes, 1, 2, out) < 2) {
            fprintf(stderr, "Failed to write to output file\n");
            exit(EXIT_FAILURE);
        }
        if (fwrite(buffer, 1, comp_len, out) < comp_len) {
            fprintf(stderr, "Failed to write to output file\n");
            exit(EXIT_FAILURE);
        }

        total += 2 + comp_len;
    }

    free(buffer);
    return total;
}

// Description: Decompress a repeat block starting at *cp.
// Params:
// - comp: compressed row data
// - row_len: bytes in compressed row
// - out_row: destination buffer
// - row_bytes: expected output bytes for the row
// - bpp: bytes per pixel
// - cp: pointer to compressed cursor
// - op: pointer to output cursor
// - repeat_count: number of times to repeat pixel
// Returns: TRUE on success, FALSE on invalid data/overflow.
int decompress_repeat_block(
    const uint8_t *comp,
    uint16_t row_len,
    uint8_t *out_row,
    size_t row_bytes,
    size_t bpp,
    size_t *cp,
    size_t *op,
    uint8_t repeat_count
) {
    if (*cp + bpp > row_len) {
        return FALSE;
    }

    // Temporary buffer for a single pixel (max 3 bytes)
    uint8_t pixel_buffer[3];
    for (size_t k = 0; k < bpp; k++) {
        pixel_buffer[k] = comp[*cp];
        *cp = *cp + 1;
    }

    size_t required = (size_t)repeat_count * bpp;
    if (*op + required > row_bytes) {
        return FALSE;
    }

    for (uint8_t r = 0; r < repeat_count; r++) {
        for (size_t k = 0; k < bpp; k++) {
            out_row[*op] = pixel_buffer[k];
            *op = *op + 1;
        }
    }

    return TRUE;
}

// Description: Decompress a literal block starting at *cp.
// Params:
// - comp: compressed row data
// - row_len: bytes in compressed row
// - out_row: destination buffer
// - row_bytes: expected output bytes for the row
// - bpp: bytes per pixel
// - cp: pointer to compressed cursor
// - op: pointer to output cursor
// Returns: TRUE on success, FALSE on invalid data/overflow.
int decompress_literal_block(
    const uint8_t *comp,
    uint16_t row_len,
    uint8_t *out_row,
    size_t row_bytes,
    size_t bpp,
    size_t *cp,
    size_t *op
) {
    if (*cp >= row_len) {
        return FALSE;
    }

    uint8_t literal_count = comp[*cp];
    *cp = *cp + 1;

    if (literal_count == 0) {
        return FALSE;
    }

    size_t needed = (size_t)literal_count * bpp;
    if (*cp + needed > row_len) {
        return FALSE;
    }
    if (*op + needed > row_bytes) {
        return FALSE;
    }

    for (uint8_t p = 0; p < literal_count; p++) {
        for (size_t k = 0; k < bpp; k++) {
            out_row[*op] = comp[*cp];
            *op = *op + 1;
            *cp = *cp + 1;
        }
    }

    return TRUE;
}

// Description: Decompress one compressed row into output buffer.
// Params:
// - comp: compressed row bytes
// - row_len: bytes in compressed row
// - out_row: destination buffer
// - row_bytes: expected output bytes for the row
// - bpp: bytes per pixel
// Returns: TRUE on success, FALSE on invalid data.
int decompress_row(
    const uint8_t *comp, 
    uint16_t row_len, uint8_t 
    *out_row, size_t row_bytes, 
    size_t bpp
) {
    // index in compressed data
    size_t cp = 0;

    // index in output row
    size_t op = 0;

    while (op < row_bytes && cp < row_len) {
        uint8_t tag = comp[cp];
        cp = cp + 1;

        int ok;
        if (tag != 0) {
            ok = decompress_repeat_block(comp, row_len, out_row,
                                         row_bytes, bpp, &cp, &op, tag);
        } else {
            ok = decompress_literal_block(comp, row_len, out_row, 
                                          row_bytes, bpp, &cp, &op);
        }
        if (!ok) {
            return FALSE;
        }
    }

    // After loop, row must be exactly filled
    if (op != row_bytes) {
        return FALSE;
    }
    return TRUE;
}

// Description: Decompress an entire RLE-compressed image.
// Params:
// - fp: input FILE* at row data
// - width: image width in pixels
// - height: image height in pixels
// - bpp: bytes per pixel
// Returns: malloc'd buffer of raw pixels; caller must free.
uint8_t *aif_decompress_image(
    FILE *fp,
    uint32_t width, 
    uint32_t height, 
    size_t bpp
) {
    size_t row_bytes = (size_t)width * bpp;
    size_t total_bytes = row_bytes * (size_t)height;
    
    // Buffer holds the entire decompressed image
    uint8_t *full_pixels = malloc(total_bytes);

    for (uint32_t row = 0; row < height; row++) {
        // Read row length (2 bytes, little-endian)
        uint8_t len_buf[2];
        if (fread(len_buf, 1, 2, fp) < 2) {
            fprintf(stderr, "Unexpected EOF\n");
            exit(EXIT_FAILURE);
        }
        uint16_t row_len = read_le_u16(len_buf);

        // Read compressed row data
        uint8_t *comp = malloc(row_len);
        if (fread(comp, 1, row_len, fp) < row_len) {
            fprintf(stderr, "Unexpected EOF\n");
            exit(EXIT_FAILURE);
        }

        // Decompress this row
        uint8_t *out_row = full_pixels + (size_t)row * row_bytes;
        
        if (!decompress_row(comp, row_len, out_row, row_bytes, bpp)) {
            fprintf(stderr, "Invalid compressed data\n");
            exit(EXIT_FAILURE);
        }
        free(comp);
    }

    return full_pixels;
}


// Description: Stage 4; decompress an RLE AIF into an uncompressed AIF.
// Params:
// - in_file: compressed input path
// - out_file: output path
// Returns: void; exits on error.
void stage4_decompress(const char *in_file, const char *out_file) {
    uint8_t header[AIF_HEADER_SIZE];
    int file_size;
    FILE *in = aif_open_and_read_header(in_file, header, &file_size);

    uint8_t pixel_format = header[AIF_PXL_FMT_OFFSET];
    uint32_t width       = read_le_u32(&header[AIF_WIDTH_OFFSET]);
    uint32_t height      = read_le_u32(&header[AIF_HEIGHT_OFFSET]);

    // Same style of validation as earlier stages
    if (!aif_magic_valid(header)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }
    if (!aif_format_valid(pixel_format)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }
    if (!aif_dim_valid(width) || !aif_dim_valid(height)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }

    size_t bpp;
    if (pixel_format == AIF_FMT_RGB8) {
        bpp = 3;
    } else {
        bpp = 1;
    }
    
    // Expand compressed input into raw pixel buffer
    uint8_t *full_pixels = aif_decompress_image(in, width, height, bpp);
    fclose(in);

    FILE *out = fopen(out_file, "wb");
    if (out == NULL) {
        fprintf(stderr, "Failed to open output file: No such file or directory\n");
        exit(EXIT_FAILURE);
    }

    size_t total_bytes = (size_t)width * bpp * (size_t)height;

    // Set compression to "none" in the header for the output image
    header[AIF_COMPRESSION_OFFSET] = AIF_COMPRESSION_NONE;

    fwrite(header, 1, AIF_HEADER_SIZE, out);
    fwrite(full_pixels, 1, total_bytes, out);

    free(full_pixels);
    fflush(out);
    fclose(out);

    FILE *rew = fopen(out_file, "r+b");
    if (rew == NULL) {
        fprintf(stderr, "Could not reopen output file\n");
        exit(EXIT_FAILURE);
    }

    int out_file_size = AIF_HEADER_SIZE + (int)total_bytes;
    uint16_t checksum = compute_checksum(rew, out_file_size);

    header[AIF_CHECKSUM_OFFSET]     = checksum & 0xFF;
    header[AIF_CHECKSUM_OFFSET + 1] = (checksum >> 8) & 0xFF;

    fseek(rew, 0, SEEK_SET);
    fwrite(header, 1, AIF_HEADER_SIZE, rew);
    fclose(rew);
}



// Description: Stage 5; compress an AIF (compressed or not) into RLE format.
// Params:
// - in_file: input path
// - out_file: output path
// Returns: void; exits on error.
void stage5_compress(const char *in_file, const char *out_file) {
    uint8_t header[AIF_HEADER_SIZE];
    int file_size;
    FILE *in = aif_open_and_read_header(in_file, header, &file_size);

    uint8_t pixel_format = header[AIF_PXL_FMT_OFFSET];
    uint8_t compression  = header[AIF_COMPRESSION_OFFSET];
    uint32_t width       = read_le_u32(&header[AIF_WIDTH_OFFSET]);
    uint32_t height      = read_le_u32(&header[AIF_HEIGHT_OFFSET]);

    if (!aif_magic_valid(header)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }
    if (!aif_format_valid(pixel_format)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }
    if (!aif_dim_valid(width) || !aif_dim_valid(height)) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }
    if (compression != AIF_COMPRESSION_NONE && compression != AIF_COMPRESSION_RLE) {
        fprintf(stderr, "'%s' is not a valid AIF file.\n", in_file);
        exit(EXIT_FAILURE);
    }

    size_t bpp;

    if (pixel_format == AIF_FMT_RGB8) {
        bpp = 3;
    } else {
        bpp = 1;
    }

    size_t pixel_bytes = bpp * (size_t)width * (size_t)height;

    // Load pixels, expanding compressed input if needed
    uint8_t *pixel_data = NULL;
    if (compression == AIF_COMPRESSION_NONE) {
        pixel_data = malloc(pixel_bytes);

        if (fread(pixel_data, 1, pixel_bytes, in) < pixel_bytes) {
            fprintf(stderr, "Unexpected EOF\n");
            exit(EXIT_FAILURE);
        }
        fclose(in);
    } else {
        pixel_data = aif_decompress_image(in, width, height, bpp);
        fclose(in);
    }

    header[AIF_COMPRESSION_OFFSET] = AIF_COMPRESSION_RLE;

    FILE *out = fopen(out_file, "wb");
    if (out == NULL) {
        fprintf(stderr, "Failed to open output file: No such file or directory\n");
        exit(EXIT_FAILURE);
    }

    fwrite(header, 1, AIF_HEADER_SIZE, out);
    size_t data_bytes = aif_write_compressed_rows(out, pixel_data, width, height, bpp);

    free(pixel_data);
    fflush(out);
    fclose(out);

    FILE *rew = fopen(out_file, "r+b");
    if (rew == NULL) {
        fprintf(stderr, "Failed to open output file: No such file or directory\n");
        exit(EXIT_FAILURE);
    }

    int out_file_size = AIF_HEADER_SIZE + (int)data_bytes;
    uint16_t checksum = compute_checksum(rew, out_file_size);

    header[AIF_CHECKSUM_OFFSET]     = checksum & 0xFF;
    header[AIF_CHECKSUM_OFFSET + 1] = (checksum >> 8) & 0xFF;

    fseek(rew, 0, SEEK_SET);
    fwrite(header, 1, AIF_HEADER_SIZE, rew);
    fclose(rew);
}

///////////////////////////////
// PROVIDED CODE
// It is best you do not modify anything below this line
///////////////////////////////
uint32_t brighten_rgb(uint32_t color, int amount) {
    uint16_t brightest_color = 0;
    uint16_t darkest_color = 255;

    for (int i = 0; i < 24; i += 8) {
        uint8_t c = ((color >> i) & 0xff);
        if (c > brightest_color) {
            brightest_color = c;
        }

        if (c < darkest_color) {
            darkest_color = c;
        }
    }

    double luminance = (
        (brightest_color + darkest_color) / 255.0
    ) / 2;

    double chroma = (
        brightest_color - darkest_color
    ) / 255.0 * 2;

    // Now that we have chroma and luminanace,
    // we can subtract the constant factor from each component
    // m = L - C / 2
    double constant = luminance - chroma / 2;

    // find the new constant
    luminance *= (1.0 + amount / 100.0);

    double adjusted = luminance - chroma / 2;

    for (int i = 0; i < 24; i += 8) {
        int16_t new_val = ((color >> i) & 0xff);

        new_val = (((new_val / 255.0) - constant) + adjusted) * 255.0;

        if (new_val > 255) {
            color |= (0xff << i);
        } else if (new_val < 0) {
            color &= ~(0xff << i);
        } else {
            color &= ~(0xff << i);
            color |= (new_val << i);
        }
    }

    return color;
}
