#pragma once

#include <stdint.h>

#if defined(_WIN32)
  #if defined(ATEM_BRIDGE_EXPORTS)
    #define ATEM_BRIDGE_API __declspec(dllexport)
  #else
    #define ATEM_BRIDGE_API __declspec(dllimport)
  #endif
#else
  #define ATEM_BRIDGE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct atem_connection atem_connection;

typedef struct atem_still_info
{
    int32_t slot;
    int32_t media_player;
    char name[128];
    char hash[33];
} atem_still_info;

ATEM_BRIDGE_API int32_t atem_connect(
    const char* device_address,
    atem_connection** out_connection,
    int32_t* out_fail_reason,
    char* error_buffer,
    int32_t error_buffer_len);

ATEM_BRIDGE_API void atem_disconnect(atem_connection* connection);

ATEM_BRIDGE_API int32_t atem_get_product_name(
    atem_connection* connection,
    char* out_name,
    int32_t out_name_len,
    char* error_buffer,
    int32_t error_buffer_len);

ATEM_BRIDGE_API int32_t atem_get_video_mode(
    atem_connection* connection,
    int32_t* out_video_mode,
    char* error_buffer,
    int32_t error_buffer_len);

ATEM_BRIDGE_API int32_t atem_get_video_dimensions(
    atem_connection* connection,
    int32_t* out_width,
    int32_t* out_height,
    char* error_buffer,
    int32_t error_buffer_len);

ATEM_BRIDGE_API int32_t atem_get_stills(
    atem_connection* connection,
    atem_still_info* out_items,
    int32_t out_items_max,
    int32_t* out_count,
    char* error_buffer,
    int32_t error_buffer_len);

ATEM_BRIDGE_API int32_t atem_upload_still_bgra(
    atem_connection* connection,
    int32_t slot_zero_based,
    const char* name,
    const uint8_t* bgra_pixels,
    int32_t pixel_count,
    int32_t width,
    int32_t height,
    char* error_buffer,
    int32_t error_buffer_len);

#ifdef __cplusplus
}
#endif
