set(COMPONENT_SRCDIRS "." "riscv")
set(COMPONENT_ADD_INCLUDEDIRS ". libretro/libretro-common/include")
set(COMPONENT_REQUIRES "retro-go" "esp_mm" "heap")
register_component()
rg_setup_compile_options(
    -O3
    -fomit-frame-pointer -ffast-math
    -DOVERCLOCK_60FPS
    -DHAVE_DYNAREC
    -DRISCV_ARCH
    -DRISCV_NATIVE_DYNAREC
    -DMMAP_JIT_CACHE
    -DROM_BUFFER_SIZE=8
    -Wno-unused-variable
    -Wno-implicit-fallthrough
    -Wno-missing-field-initializers
    -Wno-error=unused-label
    -Wno-unused-label
)

if(DEFINED ENV{GBSP_DISABLE_RISCV_NATIVE_DYNAREC})
    rg_setup_compile_options(-URISCV_NATIVE_DYNAREC)
endif()
