project(vr_player_plugin LANGUAGES CXX)

add_library(${PROJECT_NAME} SHARED
  vr_player_plugin.cc
  vr_player_plugin.h
)

apply_standard_settings(${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

target_link_libraries(${PROJECT_NAME} PRIVATE flutter)
target_link_libraries(${PROJECT_NAME} PRIVATE flutter_tizen)

# Add dependencies for Tizen Multimedia API if needed for video playback
# target_link_libraries(${PROJECT_NAME} PRIVATE capi-media-player)
