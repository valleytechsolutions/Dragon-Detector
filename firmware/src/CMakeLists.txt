file(GLOB app_sources "*.c" "*.cpp")
list(FILTER app_sources EXCLUDE REGEX "/(display|board_amoled175c|board_stopwatch|board_xiao_round|board_waveshare_lite|ui_lite|lite_support)\\.cpp$")
idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "../include")
