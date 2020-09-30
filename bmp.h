#ifndef BMP_H
#define BMP_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define UTILS_MIN(A,B) (((A)<(B))?(A):(B))
#define UTILS_MAX(A,B) (((A)>(B))?(A):(B))
#define UTILS_CLAMP(X,MIN,MAX) (UTILS_MIN(UTILS_MAX((X),(MIN)),(MAX)))

static const char *BMP_Error_Invalid_File_Descriptor =
                    "Invalid file descriptor",

                  *BMP_Error_Failed_to_Read_File_Header =
                    "Failed to read the bitmap file header",
                  *BMP_Error_Invalid_File_Signature =
                    "Invalid bitmap file signature",
                  *BMP_Error_Failed_to_Read_DIB_Header =
                    "Failed to read the DIB header",
                  *BMP_Error_Invalid_Dib_Header_Size =
                    "Invalid DIB header size",
                  *BMP_Error_Unsupported_Color_Depth =
                    "Invalid color depth (not 24 or 32 bits per pixel)",
                  *BMP_Error_Invalid_Size_Information =
                    "The bitmap image containes invalid size information",

                  *BMP_Error_Invalid_Image_Structure =
                    "Invalid bitmap image structure",
                  *BMP_Error_Not_Enough_Memory_to_Read =
                    "Not enough memory to read the image",
                  *BMP_Error_Failed_to_Read_Image_Data =
                    "Failed to read the image data",
                  *BMP_Error_Invalid_Pixel_Offset_or_DIB_Header_Size =
                    "Invalid pixel offset or DIB header size",
                  *BMP_Error_Failed_to_Calculate_Padding =
                    "Failed to calculate padding information",

                  *BMP_Error_Failed_to_Write_File_Header =
                    "Failed to write the bitmap file header",
                  *BMP_Error_Failed_to_Write_DIB_Header =
                    "Failed to write the DIB header",

                  *BMP_Error_Failed_to_Write_Image_Data =
                    "Failed to write the image data";

static const int BMP_First_Magic_Byte  = 0x42,
                 BMP_Second_Magic_Byte = 0x4D;

struct _bmp_file_header
{
    uint8_t  signature[2];
    uint32_t file_size;
    uint16_t reserved[2];
    uint32_t pixel_array_offset;
} __attribute__((packed));

typedef struct _bmp_file_header bmp_file_header;

struct _bmp_dib_header
{
    uint32_t dib_header_size;
    int32_t  image_width;
    int32_t  image_height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_pixels_per_meter;
    int32_t  y_pixels_per_meter;
    uint32_t colors_in_color_table;
    uint32_t important_color_count;
} __attribute__((packed));

typedef struct _bmp_dib_header bmp_dib_header;

#define REST_OF_DIB_HEADER_SIZE 256
typedef struct _bmp_image
{
    bmp_file_header file_header;
    bmp_dib_header dib_header;
    uint8_t rest_of_dib_header[REST_OF_DIB_HEADER_SIZE];

    size_t payload_size;
    uint8_t *payload;

    /* Convenience Variables */
    uint8_t *raw_pixels;            /* start of pixel array in the payload                                           */
    uint8_t *pixels;                /* start of pixel array without padding aligned on a 64-bit boundary             */
    size_t absolute_image_width;    /* abs(dib_header.image_width)                                                   */
    size_t absolute_image_height;   /* abs(dib_header.image_height)                                                  */
    size_t pixel_row_padding;       /* the padding after each row of pixels                                          */
    size_t image_size;              /* the total size of the image part in bytes                                     */
    size_t aligned_image_size;      /* the total size of the aligned image part without padding                      */
    size_t channels;                /* channel count (3 for 24-bit images, 4 for 32-bit images with an alpha channel */
} bmp_image;

static inline void bmp_init_image_structure(bmp_image *image)
{
    if (NULL != image) {
        memset(image, 0, sizeof(*image));
    }
}

static inline void bmp_free_image_structure(bmp_image *image)
{
    if (NULL != image) {
        if (NULL != image->payload) {
            free(image->payload);
            image->payload = NULL;
        }
        if (NULL != image->pixels) {
            free(image->pixels);
            image->pixels = NULL;
        }
    }
}

static void bmp_open_image_headers(
                FILE *file_descriptor,
                bmp_image *image,
                const char **error_message
            )
{
    *error_message = NULL;

    if (NULL == image) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_Image_Structure;
        }

        goto end;
    }

    if (NULL == file_descriptor) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_File_Descriptor;
        }

        goto end;
    }

    size_t bmp_header_size =
        sizeof(image->file_header);
    size_t dib_header_size =
        sizeof(image->dib_header);
    size_t total_header_size =
        bmp_header_size + dib_header_size;

    if (!fread(&image->file_header, bmp_header_size, 1, file_descriptor)) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Failed_to_Read_File_Header;
        }

        goto end;
    }

    if (image->file_header.signature[0] != BMP_First_Magic_Byte ||
        image->file_header.signature[1] != BMP_Second_Magic_Byte) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_File_Signature;
        }

        goto end;
    }

    if (!fread(&image->dib_header, dib_header_size, 1, file_descriptor)) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Failed_to_Read_DIB_Header;
        }

        goto end;
    }

    ssize_t temp =
        image->dib_header.dib_header_size - dib_header_size;
    size_t rest_dib_header_size =
        temp > 0 ? temp : 0;

    if (rest_dib_header_size > REST_OF_DIB_HEADER_SIZE) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_Dib_Header_Size;
        }

        goto end;
    }

    if (rest_dib_header_size > 0) {
        if (!fread(image->rest_of_dib_header, rest_dib_header_size, 1, file_descriptor)) {
            if (NULL != error_message) {
                *error_message = BMP_Error_Failed_to_Read_DIB_Header;
            }

            goto end;
        }
    }

    if (24 != image->dib_header.bits_per_pixel &&
        32 != image->dib_header.bits_per_pixel) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Unsupported_Color_Depth;
        }

        goto end;
    }
    image->channels = 24 == image->dib_header.bits_per_pixel ? 3 : 4;

    if (image->file_header.file_size <= total_header_size) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_Size_Information;
        }

        goto end;
    }

end:
    return;
}

static void bmp_read_image_data(
                FILE *file_descriptor,
                bmp_image *image,
                const char **error_message
            )
{
    *error_message = NULL;

    if (NULL == image) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_Image_Structure;
        }

        goto end;
    }

    if (NULL == file_descriptor) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_File_Descriptor;
        }

        goto end;
    }

    size_t bmp_header_size =
        sizeof(image->file_header);
    size_t dib_header_size =
        image->dib_header.dib_header_size;
    size_t total_header_size =
        bmp_header_size + dib_header_size;

    size_t payload_size =
        ((size_t) image->file_header.file_size) - total_header_size;

    image->payload_size = payload_size;
    image->payload = (uint8_t *) malloc(payload_size);
    if (NULL == image->payload) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Not_Enough_Memory_to_Read;
        }

        goto end;
    }

    if (!fread(image->payload, payload_size, 1, file_descriptor)) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Failed_to_Read_Image_Data;
        }

        goto cleanup;
    }

    size_t first_pixel_index =
        ((size_t) image->file_header.pixel_array_offset) -
            (bmp_header_size + (size_t) image->dib_header.dib_header_size);

    if (first_pixel_index >= payload_size) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_Pixel_Offset_or_DIB_Header_Size;
        }

        goto cleanup;
    }

    image->raw_pixels =
        &image->payload[first_pixel_index];

    size_t width =
        image->dib_header.image_width < 0 ?
            (size_t) -image->dib_header.image_width :
            (size_t)  image->dib_header.image_width;

    size_t height =
        image->dib_header.image_height < 0 ?
            (size_t) -image->dib_header.image_height :
            (size_t)  image->dib_header.image_height;

    size_t row_size_extend_to_4 =
        width * 4;
    size_t row_size =
        width * image->channels;

    size_t padding = (size_t) image->dib_header.bits_per_pixel;
    padding = (padding * width + 31) / 32 * 4 - row_size;

    image->absolute_image_width  =
        width;
    image->absolute_image_height =
        height;
    image->pixel_row_padding =
        padding;
    image->image_size =
        height * (row_size + padding);

    if (image->image_size > payload_size) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Failed_to_Calculate_Padding;
        }

        goto cleanup;
    }

    size_t extended_to_4_image_size =
        height * (width * 4 + padding);

    size_t alignment = 64;
    size_t aligned_image_size = (((extended_to_4_image_size - 1) / alignment) + 1) * alignment;
    aligned_image_size += alignment;

    image->pixels = (uint8_t *) aligned_alloc(64, aligned_image_size);
    if (NULL == image->pixels) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Not_Enough_Memory_to_Read;
        }

        goto cleanup;
    }
    image->aligned_image_size = aligned_image_size;

    if (4 == image->channels) {
        for (
            size_t y = 0,
                   src_linear_position  = 0,
                   dest_linear_position = 0;
            y < height;
            ++y,
            src_linear_position += row_size + padding,
            dest_linear_position += row_size
        ) {
            memcpy(
                image->pixels + dest_linear_position,
                image->raw_pixels + src_linear_position,
                row_size
            );
        }
    } else {
        for (
            size_t y = 0,
                   src_linear_position  = 0,
                   dest_linear_position = 0;
            y < height;
            ++y,
            src_linear_position += row_size + padding,
            dest_linear_position += row_size_extend_to_4
        ) {
            uint8_t *source = image->raw_pixels + src_linear_position;
            uint8_t *destination = image->pixels + dest_linear_position;
            for (int i = 0, j = 0; i < row_size; i += 3, j += 4) {
                uint8_t *target_pixel = destination + j;
                memcpy(
                    target_pixel,
                    source + i,
                    3
                );
                *(target_pixel + 3) = 255;
            }
        }
    }

    for (size_t linear_position = extended_to_4_image_size; linear_position < aligned_image_size; ++linear_position) {
        image->pixels[linear_position] = 0;
    }

end:
    return;

cleanup:
    if (NULL != image->payload)
    {
        free(image->payload);
        image->payload = NULL;
    }
    if (NULL != image->pixels)
    {
        free(image->pixels);
        image->pixels = NULL;
    }
}

static void bmp_write_image_headers(
                FILE *file_descriptor,
                bmp_image *image,
                const char **error_message
            )
{
    *error_message = NULL;

    if (NULL == image) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_Image_Structure;
        }

        goto end;
    }

    if (NULL == file_descriptor) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_File_Descriptor;
        }

        goto end;
    }

    size_t bmp_header_size =
        sizeof(image->file_header);

    if (!fwrite(&image->file_header, bmp_header_size, 1, file_descriptor)) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Failed_to_Write_File_Header;
        }

        goto end;
    }

    size_t dib_header_size =
        image->dib_header.dib_header_size;

    if (!fwrite(&image->dib_header, dib_header_size, 1, file_descriptor)) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Failed_to_Write_DIB_Header;
        }

        goto end;
    }

end:
    return;
}

static void bmp_write_image_data(
                FILE *file_descriptor,
                bmp_image *image,
                const char **error_message
            )
{
    *error_message = NULL;

    if (NULL == image) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_Image_Structure;
        }

        goto end;
    }

    if (NULL == file_descriptor) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Invalid_File_Descriptor;
        }

        goto end;
    }

    size_t bmp_header_size =
        sizeof(image->file_header);
    size_t dib_header_size =
        image->dib_header.dib_header_size;
    size_t total_header_size =
        bmp_header_size + dib_header_size;

    size_t payload_size =
        ((size_t) image->file_header.file_size) - total_header_size;
    size_t padding =
        image->pixel_row_padding;

    size_t width =
        image->absolute_image_width;
    size_t height =
        image->absolute_image_height;
    size_t row_size_extend_to_4 =
        width * 4;
    size_t row_size =
        width * image->channels;

    if (4 == image->channels) {
        for (
            size_t y = 0,
                   src_linear_position  = 0,
                   dest_linear_position = 0;
            y < height;
            ++y,
            src_linear_position += row_size,
            dest_linear_position += row_size + padding
        ) {
            memcpy(
                image->raw_pixels + dest_linear_position,
                image->pixels + src_linear_position,
                row_size
            );
        }
    } else {
        for (
            size_t y = 0,
                   src_linear_position  = 0,
                   dest_linear_position = 0;
            y < height;
            ++y,
            src_linear_position += row_size_extend_to_4,
            dest_linear_position += row_size + padding
        ) {
            uint8_t *source = image->pixels + src_linear_position;
            uint8_t *destination = image->raw_pixels + dest_linear_position;
            for (int i = 0, j = 0; i < row_size; i += 3, j += 4) {
                memcpy(
                    destination + i,
                    source + j,
                    3
                );
            }
        }
    }

    if (!fwrite(image->payload, payload_size, 1, file_descriptor)) {
        if (NULL != error_message) {
            *error_message = BMP_Error_Failed_to_Write_Image_Data;
        }

        goto end;
    }

end:
    return;
}

static inline uint8_t *bmp_sample_pixel(
                           uint8_t *pixels,
                           ssize_t x,
                           ssize_t y,
                           size_t absolute_image_width,
                           size_t absolute_image_height
                       )
{
    size_t ux =
        (size_t) (UTILS_CLAMP(x, 0, (ssize_t) absolute_image_width - 1));
    size_t uy =
        (size_t) (UTILS_CLAMP(y, 0, (ssize_t) absolute_image_height - 1));

    return &pixels[uy * (absolute_image_width * 4) + ux * 4];
}

static inline uint8_t *bmp_sample_raw_pixel(
                           uint8_t *raw_pixels,
                           ssize_t x,
                           ssize_t y,
                           size_t absolute_image_width,
                           size_t absolute_image_height,
                           size_t row_padding
                       )
{
    size_t ux =
        (size_t) (UTILS_CLAMP(x, 0, (ssize_t) absolute_image_width - 1));
    size_t uy =
        (size_t) (UTILS_CLAMP(y, 0, (ssize_t) absolute_image_height - 1));

    return &raw_pixels[uy * (absolute_image_width * 4 + row_padding) + ux * 4];
}

#endif /* BMP_H */

