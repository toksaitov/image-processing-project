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
        fprintf(stderr, "Usage: %s <source file> <dest. file>\n", argv[0]);
        return result;
    }

    char *source_file_name = argv[1];
    char *destination_file_name = argv[2];
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

            static const float Sepia_Coefficients[] = {
                0.272f, 0.534f, 0.131f,
                0.349f, 0.686f, 0.168f,
                0.393f, 0.769f, 0.189f
            };

            uint32_t blue =
                pixels[position];
            uint32_t green =
                pixels[position + 1];
            uint32_t red =
                pixels[position + 2];

            pixels[position] =
                (uint8_t) UTILS_MIN(
                              Sepia_Coefficients[0] * blue  +
                              Sepia_Coefficients[1] * green +
                              Sepia_Coefficients[2] * red,
                              255.0f
                          );
            pixels[position + 1] =
                (uint8_t) UTILS_MIN(
                              Sepia_Coefficients[3] * blue  +
                              Sepia_Coefficients[4] * green +
                              Sepia_Coefficients[5] * red,
                              255.0f
                          );
            pixels[position + 2] =
                (uint8_t) UTILS_MIN(
                              Sepia_Coefficients[6] * blue  +
                              Sepia_Coefficients[7] * green +
                              Sepia_Coefficients[8] * red,
                              255.0f
                          );

#elif defined SIMD_INTRINSICS_IMPLEMENTATION

            static const float Sepia_Coefficients[] = {
                0.272f, 0.349f, 0.393f, 1.0f, 0.272f, 0.349f, 0.393f, 1.0f, 0.272f, 0.349f, 0.393f, 1.0f, 0.272f, 0.349f, 0.393f, 1.0f,
                0.534f, 0.686f, 0.769f, 1.0f, 0.534f, 0.686f, 0.769f, 1.0f, 0.534f, 0.686f, 0.769f, 1.0f, 0.534f, 0.686f, 0.769f, 1.0f,
                0.131f, 0.168f, 0.189f, 1.0f, 0.131f, 0.168f, 0.189f, 1.0f, 0.131f, 0.168f, 0.189f, 1.0f, 0.131f, 0.168f, 0.189f, 1.0f
            };

            __m512 coeff1 = _mm512_load_ps(&Sepia_Coefficients[0]);
            __m512 coeff2 = _mm512_load_ps(&Sepia_Coefficients[16]);
            __m512 coeff3 = _mm512_load_ps(&Sepia_Coefficients[32]);
            __m512i ints = _mm512_cvtepu8_epi32(_mm_load_si128((__m128i *) &pixels[position]));
            __m512 floats = _mm512_cvtepi32_ps(ints);
            __m512 temp1 = floats;
            __m512 temp2 = floats;
            __m512 temp3 = floats;
            temp1 = _mm512_permute_ps(temp1, 0b11000001);
            temp2 = _mm512_permute_ps(temp2, 0b11010101);
            temp3 = _mm512_permute_ps(temp3, 0b11101010);
            floats = _mm512_mul_ps(coeff1, temp1);
            floats = _mm512_fmadd_ps(coeff2, temp2, floats);
            floats = _mm512_fmadd_ps(coeff3, temp3, floats);
            ints = _mm512_cvtps_epi32(floats);
            _mm512_mask_cvtusepi32_storeu_epi8(&pixels[position], 0xffff, ints);

#elif defined SIMD_ASM_IMPLEMENTATION

            /*
                Write the inline assembly representation of the intrinsics above
                in SIMD_INTRINSICS_IMPLEMENTATION here in SIMD_ASM_IMPLEMENTATION.
            */

            // TODO

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
