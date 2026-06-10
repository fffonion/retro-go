# Espressif chip in the device
IDF_TARGET = "esp32s31"

# .fw file format, if supported by the device
FW_FORMAT = "none"

# Default app set for S31 Retro-Go bring-up.
DEFAULT_APPS = "launcher retro-core"

# S31 flash is large enough; keep target image partitions aligned with the
# 3 MB per-app build partition used by rg_tool.py so packed images fit too.
globals()["PROJECT_APPS"]["screen-test"][2] = 0x100000
globals()["PROJECT_APPS"]["touch-test"][2] = 0x100000
globals()["PROJECT_APPS"]["input-test"][2] = 0x100000
globals()["PROJECT_APPS"]["audio-test"][2] = 0x100000
globals()["PROJECT_APPS"]["launcher"][2] = 0x300000
globals()["PROJECT_APPS"]["retro-core"][2] = 0x300000
globals()["PROJECT_APPS"]["gwenesis"][2] = 0x300000
globals()["PROJECT_APPS"]["fmsx"][2] = 0x300000
