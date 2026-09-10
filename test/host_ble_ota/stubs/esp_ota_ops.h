#pragma once
#include <cstddef>
#include <cstdint>
using esp_err_t = int;
using esp_ota_handle_t = uint32_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_ERR_OTA_VALIDATE_FAILED = 1;
struct esp_partition_t { uint32_t size = 4096; int subtype = 1; unsigned address = 0x10000; };
const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t*);
esp_err_t esp_ota_begin(const esp_partition_t*, size_t, esp_ota_handle_t*);
esp_err_t esp_ota_write(esp_ota_handle_t, const void*, size_t);
esp_err_t esp_ota_end(esp_ota_handle_t);
esp_err_t esp_ota_abort(esp_ota_handle_t);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t*);
inline const char* esp_err_to_name(esp_err_t) { return "test error"; }
