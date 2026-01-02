#include "aif.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_OPS 5

void stage2_brighten_args(int n_args, const char **args);
void stage3_convert_color_args(int n_args, const char **args);
void stage4_decompress_args(int n_args, const char **args);
void stage5_compress_args(int n_args, const char **args);

struct aif_operation {
    const char *name;
    void(*operation)(int n_files, const char **files);
};

const struct aif_operation operations[] = {
    {"info", stage1_info},
    {"brighten", stage2_brighten_args},
    {"convert-color", stage3_convert_color_args},
    {"decompress", stage4_decompress_args},
    {"compress", stage5_compress_args},
};

int main(int argc, const char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: aif-tools <info|brighten|compress> file1 [... <file2>]\n");
        return 1;
    } else if (argc == 2) {
        fprintf(stderr, "No input files provided\n");
        return 1;
    }

    for (int i = 0; i < NUM_OPS; i++) {
        if (strcmp(argv[1], operations[i].name) == 0) {
            operations[i].operation(argc - 2, argv + 2);
            return 0;
        }
    }

    fprintf(stderr, "Unknown operation: %s\n", argv[1]);

    return 1;
}

void stage2_brighten_args(int n_args, const char **args) {
    if (n_args < 3) {
        fprintf(stderr, "Usage: aif-tools brighten <amount> <in-file> <out-file>\n");
        exit(EXIT_FAILURE);
    }

    int amount = atoi(args[0]);
    if (amount < -100 || amount > 100) {
        fprintf(stderr, "Amount must be between -100 and 100\n");
        exit(EXIT_FAILURE);
    }

    stage2_brighten(amount, args[1], args[2]);
}

void stage3_convert_color_args(int n_args, const char **args) {
    if (n_args < 3) {
        fprintf(
            stderr,
            "Usage: aif-tools convert-color <color-format> <in-file> <out-file>\n"
        );
        exit(EXIT_FAILURE);
    }

    stage3_convert_color(args[0], args[1], args[2]);
}

void stage4_decompress_args(int n_args, const char **args) {
    if (n_args < 2) {
        fprintf(stderr, "Usage: aif-tools decompress <in-file> <out-file>\n");
        exit(EXIT_FAILURE);
    }

    stage4_decompress(args[0], args[1]);
}

void stage5_compress_args(int n_args, const char **args) {
    if (n_args < 2) {
        fprintf(stderr, "Usage: aif-tools compress <in-file> <out-file>\n");
        exit(EXIT_FAILURE);
    }

    stage5_compress(args[0], args[1]);
}

int aif_pixel_format_bpp(int format) {
    switch(format) {
    case AIF_FMT_RGB8:
        return 24;
    case AIF_FMT_GRAY8:
        return 8;
    default:
        return -1;
    }
}

const char *aif_pixel_format_name(int format) {
    switch(format) {
    case AIF_FMT_RGB8:
        return "8-bit RGB";
    case AIF_FMT_GRAY8:
        return "8-bit grayscale";
    default:
        return NULL;
    }
}

const char *aif_compression_name(int compression) {
    switch(compression) {
    case AIF_COMPRESSION_NONE:
        return "none";
    case AIF_COMPRESSION_RLE:
        return "run-length encoding compressed";
    default:
        return NULL;
    }
}
