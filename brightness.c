#include "bmp.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <immintrin.h>

#if !defined C_IMPLEMENTATION && \
    !defined SIMD_INTRINSICS_IMPLEMENTATION && \
    !defined SIMD_ASM_IMPLEMENTATION
#define C_IMPLEMENTATION 1
#endif

int main(int argc, char *argv[])
{
    int result = EXIT_FAILURE;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <brightness> <contrast> <source file> <dest. file>\n", argv[0]);
        return result;
    }

    float brightness = strtof(argv[1], NULL);
    float contrast = strtof(argv[2], NULL);

    char *source_file_name = argv[3];
    char *destination_file_name = argv[4];
    FILE *source_descriptor = NULL;
    FILE *destination_descriptor = NULL;

    bmp_image image; bmp_init_image_structure(&image);

    source_descriptor = fopen(source_file_name, "r");
    if (source_descriptor == NULL) {
        fprintf(stderr, "Failed to open the source image file '%s'\n", source_file_name);
        goto cleanup;
    }

    const char *error_message;
    bmp_open_image_headers(source_descriptor, &image, &error_message);
    if (error_message != NULL) {
        fprintf(stderr, "Failed to process the image '%s':\n\t%s\n", source_file_name, error_message);
        goto cleanup;
    }

    bmp_read_image_data(source_descriptor, &image, &error_message);
    if (error_message != NULL) {
        fprintf(stderr, "Failed to process the image '%s':\n\t%s\n", source_file_name, error_message);
        goto cleanup;
    }

    destination_descriptor = fopen(destination_file_name, "w");
    if (destination_descriptor == NULL) {
        fprintf(stderr, "Failed to create the output image '%s'\n", destination_file_name);
        goto cleanup;
    }

    bmp_write_image_headers(destination_descriptor, &image, &error_message);
    if (error_message != NULL) {
        fprintf(stderr, "Failed to process the image '%s':\n\t%s\n", destination_file_name, error_message);
        goto cleanup;
    }

    /* Main Image Processing Loop */
    {
        uint8_t *pixels = image.pixels;

        size_t width = image.absolute_image_width;
        size_t height = image.absolute_image_height;

        size_t channels_count = width * height * 4;

#if defined SIMD_INTRINSICS_IMPLEMENTATION || defined SIMD_ASM_IMPLEMENTATION
        size_t step = 16;
#else
        size_t step = 4;
#endif

        for (size_t position = 0; position < channels_count; position += step) {
#if defined C_IMPLEMENTATION

            pixels[position] =
                (uint8_t) UTILS_CLAMP(pixels[position] * contrast + brightness, 0.0f, 255.0f);
            pixels[position + 1] =
                (uint8_t) UTILS_CLAMP(pixels[position + 1] * contrast + brightness, 0.0f, 255.0f);
            pixels[position + 2] =
                (uint8_t) UTILS_CLAMP(pixels[position + 2] * contrast + brightness, 0.0f, 255.0f);

#elif defined SIMD_INTRINSICS_IMPLEMENTATION

            /*
                Write the intrinsics for the SIMD assembly bellow in
                SIMD_ASM_IMPLEMENTATION here in SIMD_INTRINSICS_IMPLEMENTATION.
            */

            // TODO

#elif defined SIMD_ASM_IMPLEMENTATION

            __asm__ __volatile__ (
                "vbroadcastss (%0), %%zmm2\n\t"
                "vbroadcastss (%1), %%zmm1\n\t"
                "vpmovzxbd (%2,%3), %%zmm0\n\t"
                "vcvtdq2ps %%zmm0, %%zmm0\n\t"
                "vfmadd132ps %%zmm1, %%zmm2, %%zmm0\n\t"
                "vcvtps2dq %%zmm0, %%zmm0\n\t"
                "vpmovusdb %%zmm0, (%2,%3)\n\t"
            ::
                "S"(&brightness), "D"(&contrast), "b"(pixels), "c"(position)
            :
                "%zmm0", "%zmm1", "%zmm2"
            );

#endif
        }
    }

    bmp_write_image_data(destination_descriptor, &image, &error_message);
    if (error_message != NULL) {
        fprintf(stderr, "Failed to process the image '%s':\n\t%s\n", destination_file_name, error_message);
        goto cleanup;
    }

    result = EXIT_SUCCESS;

cleanup:
    bmp_free_image_structure(&image);

    if (source_descriptor != NULL) {
        fclose(source_descriptor);
        source_descriptor = NULL;
    }

    if (destination_descriptor != NULL) {
        fclose(destination_descriptor);
        destination_descriptor = NULL;
    }

    return result;
}
