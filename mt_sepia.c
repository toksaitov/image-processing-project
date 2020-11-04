#include "bmp.h"
#include "threadpool.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined C_IMPLEMENTATION && \
    !defined SIMD_INTRINSICS_IMPLEMENTATION && \
    !defined SIMD_ASM_IMPLEMENTATION
#define C_IMPLEMENTATION 1
#endif

#ifndef C_IMPLEMENTATION
#include <immintrin.h>
#endif

typedef struct _filters_sepia_data
{
    uint8_t *pixels;
    size_t position;
    size_t channels_to_process;
    volatile ssize_t *channels_left;
    volatile bool *barrier_sense;
} filters_sepia_data_t;

static void sepia_processing_task(
                void *task_data,
                void (*result_callback)(void *result) __attribute__((unused))
            )
{
    filters_sepia_data_t *data = task_data;

    uint8_t *pixels = data->pixels;
    size_t position = data->position;
    size_t channels_to_process = data->channels_to_process;
    size_t end = position + channels_to_process;
#if defined SIMD_INTRINSICS_IMPLEMENTATION || defined SIMD_ASM_IMPLEMENTATION
    size_t step = 16;
#else
    size_t step = 4;
#endif

    for (; position < end; position += step) {
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

        static const float Sepia_Coefficients[] __attribute__((aligned(0x40))) = {
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
        temp1 = _mm512_permute_ps(temp1, 0b11000000);
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

    ssize_t channels_left = __sync_sub_and_fetch(data->channels_left, (ssize_t) channels_to_process);
    if (channels_left <= 0) {
        __sync_lock_test_and_set(data->barrier_sense, true);
    }

    free(data);
    data = NULL;
}

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
        perror(NULL);
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

    size_t pool_size = utils_get_number_of_cpu_cores();
    threadpool_t *threadpool = threadpool_create(pool_size);
    if (threadpool == NULL) {
        fputs("Failed to create a threadpool.\n", stderr);
        goto cleanup;
    }

    /* Main Image Processing Loop */
    {
        static volatile ssize_t channels_left = 0;
        static volatile bool barrier_sense = false;

        uint8_t *pixels = image.pixels;

        size_t width = image.absolute_image_width;
        size_t height = image.absolute_image_height;

        size_t channels_count = width * height * 4;
        channels_left = channels_count;
        size_t channels_per_thread = channels_count / pool_size;

#if defined SIMD_INTRINSICS_IMPLEMENTATION || defined SIMD_ASM_IMPLEMENTATION
        channels_per_thread = ((channels_per_thread - 1) / 16 + 1) * 16;
#else
        channels_per_thread = ((channels_per_thread - 1) / 4 + 1) * 4;
#endif

        for (size_t position = 0; position < channels_count; position += channels_per_thread) {
            filters_sepia_data_t *task_data = malloc(sizeof(*task_data));
            if (task_data == NULL) {
                fputs("Out of memory.\n", stderr);
                goto cleanup;
            }

            task_data->pixels = pixels;
            task_data->position = position;

            task_data->channels_to_process =
                position + channels_per_thread > channels_count ?
                    channels_count - position :
                    channels_per_thread;
            task_data->channels_left = &channels_left;
            task_data->barrier_sense = &barrier_sense;

            threadpool_enqueue_task(threadpool, sepia_processing_task, task_data, NULL);
        }

        while (!barrier_sense) { }
        threadpool_destroy(threadpool);    
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
