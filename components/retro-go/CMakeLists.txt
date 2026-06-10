set(COMPONENT_SRCDIRS ". drivers/audio fonts libs/cJSON libs/lodepng")
set(COMPONENT_ADD_INCLUDEDIRS ". libs/cJSON libs/lodepng")
if(IDF_VERSION_MAJOR GREATER 5) # esp-idf 6.x+
    set(COMPONENT_REQUIRES "spi_flash fatfs app_update nvs_flash esp_adc esp_timer esp_psram sdmmc esp_driver_gpio esp_driver_i2s esp_driver_sdmmc esp_driver_sdspi esp_driver_spi esp_driver_ledc esp_lcd esp_driver_ppa driver")
elseif((IDF_VERSION_MAJOR EQUAL 5) AND (IDF_VERSION_MINOR GREATER 2)) # esp-idf 5.3+
    set(COMPONENT_REQUIRES "spi_flash fatfs app_update nvs_flash esp_adc esp_timer esp_psram esp_driver_ledc driver")
elseif(IDF_VERSION_MAJOR EQUAL 5) # esp-idf 5.0 - 5.2
    set(COMPONENT_REQUIRES "spi_flash fatfs app_update nvs_flash esp_adc esp_timer esp_psram")
else() # esp-idf 4.1 - 4.4
    set(COMPONENT_REQUIRES "spi_flash fatfs app_update esp_adc_cal nvs_flash")
endif()

if(RG_ENABLE_NETWORKING OR RG_ENABLE_NETPLAY)
    list(APPEND COMPONENT_REQUIRES esp_wifi esp_http_client)
endif()

if(RG_BUILD_TARGET STREQUAL "RG_TARGET_ESP32_S31_KORVO_1")
    list(APPEND COMPONENT_REQUIRES esp_codec_dev)
endif()

if(RG_ENABLE_NETPLAY)
    list(APPEND COMPONENT_SRCDIRS libs/netplay)
    list(APPEND COMPONENT_ADD_INCLUDEDIRS libs/netplay)
endif()

register_component()

# Small size is preferred because of the small cache and most things here aren't performance sensitive!
# rg_display and rg_audio benefit from higher optimization (which of -O2 or -O3 is better depends...)

component_compile_options(
    -DLODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
    -DLODEPNG_NO_COMPILE_ERROR_TEXT
    -Os
    -Wno-unused-function
)
set_source_files_properties(
    rg_audio.c rg_display.c rg_utils.c rg_surface.c libs/lodepng/lodepng.c
    PROPERTIES COMPILE_FLAGS
    -O2
)

if(RG_ENABLE_NETPLAY)
    component_compile_options(-DRG_ENABLE_NETWORKING -DRG_ENABLE_NETPLAY)
elseif(RG_ENABLE_NETWORKING)
    component_compile_options(-DRG_ENABLE_NETWORKING)
endif()

if(RG_ENABLE_PROFILING)
    component_compile_options(-DRG_ENABLE_PROFILING)
endif()

if(RG_PROJECT_VER)
    component_compile_options(-DRG_PROJECT_VER="${RG_PROJECT_VER}")
endif()

if(RG_BUILD_RELEASE)
    component_compile_options(-DRG_BUILD_RELEASE=1)
endif()

set(RG_BUILD_INFO "${RG_BUILD_TARGET}-${RG_BUILD_RELEASE} ${PROJECT_VER} SDK:${IDF_VER} ${IDF_TARGET}")
string(TIMESTAMP RG_BUILD_TIME "%s")

component_compile_options(-D${RG_BUILD_TARGET}=1)
component_compile_options(-DRG_PROJECT_APP="${RG_PROJECT_APP}")
component_compile_options(-DRG_BUILD_INFO="${RG_BUILD_INFO}")
component_compile_options(-DRG_BUILD_TIME=${RG_BUILD_TIME})

message(STATUS "RG_PROJECT_APP: ${RG_PROJECT_APP}")
message(STATUS "RG_PROJECT_VER: ${RG_PROJECT_VER}")
message(STATUS "RG_BUILD_INFO: ${RG_BUILD_INFO}")
