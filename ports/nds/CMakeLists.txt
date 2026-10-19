cmake_minimum_required(VERSION 3.14)
project(revc_nds LANGUAGES C CXX)
option(REVC_NDS_HOST_VALIDATION "Build native asset-loader validation instead of a ROM" OFF)
set(REVC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../external/reVC")
if(NOT EXISTS "${REVC_ROOT}/vendor/librw/rw.h")
    message(FATAL_ERROR "Clone reVC and its submodules into external/reVC first")
endif()
set(LIBRW_PLATFORM NULL CACHE STRING "Asset-reader backend" FORCE)
set(LIBRW_TOOLS OFF CACHE BOOL "" FORCE)
set(LIBRW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${REVC_ROOT}/vendor/librw" librw)
target_compile_definitions(librw PUBLIC REVC_NDS)
target_compile_options(librw PRIVATE -ffunction-sections -fdata-sections)

file(GLOB_RECURSE REVC_SOURCES CONFIGURE_DEPENDS "${REVC_ROOT}/src/*.cpp")
list(FILTER REVC_SOURCES EXCLUDE REGEX "/skel/(android|glfw|sdl2|win)/")
list(FILTER REVC_SOURCES EXCLUDE REGEX "/CdStream_(posix|win)\\.cpp$")
list(FILTER REVC_SOURCES EXCLUDE REGEX "/sampman_(oal|miles)\\.cpp$")
list(FILTER REVC_SOURCES EXCLUDE REGEX "/audio/eax/")
list(FILTER REVC_SOURCES EXCLUDE REGEX "/GitSHA1\\.cpp$")
file(GLOB_RECURSE HEADERS "${REVC_ROOT}/src/*.h")
set(INCLUDE_DIRS "${REVC_ROOT}")
foreach(HEADER IN LISTS HEADERS)
    get_filename_component(DIRECTORY "${HEADER}" DIRECTORY)
    list(APPEND INCLUDE_DIRS "${DIRECTORY}")
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

# This is an archive build for porting; unresolved platform functions are not
# hidden with stubs. Only a final game link can establish a complete port.
add_library(revc_core STATIC ${REVC_SOURCES} input.cpp cdstream.cpp)
target_include_directories(revc_core PUBLIC ${INCLUDE_DIRS})
target_compile_definitions(revc_core PUBLIC LIBRW RW_NULL RWDEVICE=ps2 REVC_NDS CMAKE_BUILD USE_OUR_VERSIONING MASTER)
target_compile_options(revc_core PRIVATE -Wno-narrowing -Wno-register -Wno-write-strings
                                       -ffunction-sections -fdata-sections -fno-strict-aliasing)
target_link_libraries(revc_core PUBLIC librw::librw)
set_target_properties(revc_core PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
list(LENGTH REVC_SOURCES SOURCE_COUNT)
message(STATUS "ARM9 engine archive: ${SOURCE_COUNT} upstream translation units")

if(REVC_NDS_HOST_VALIDATION)
    # Validation expressions must execute even in MinSizeRel/Release builds.
    add_compile_options(-UNDEBUG)
    add_executable(revc_cinematic_gameplay_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/cinematic_gameplay_test.cpp" animation.cpp)
    target_include_directories(revc_cinematic_gameplay_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_moving_visibility_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/moving_visibility_test.cpp" gameplay.cpp world.cpp packed.cpp)
    target_include_directories(revc_moving_visibility_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_spatial_streaming_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/spatial_streaming_test.cpp" gameplay.cpp world.cpp packed.cpp)
    target_include_directories(revc_spatial_streaming_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_cinematic_frame_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/cinematic_frame_test.cpp" cinematic_frame.cpp packed.cpp)
    target_include_directories(revc_cinematic_frame_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_render_quality_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/render_quality_test.cpp" textures.cpp lighting.cpp packed.cpp)
    target_include_directories(revc_render_quality_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_weapon_wheel_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/weapon_wheel_test.cpp")
    target_include_directories(revc_weapon_wheel_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_cinematic_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/cinematic_test.cpp" cinematic.cpp cinematic_frame.cpp packed.cpp)
    target_include_directories(revc_cinematic_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../../tests/nds_stub" "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_assets_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/revc_assets_test.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/../../tests/host_platform.cpp" sector.cpp)
    target_include_directories(revc_assets_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(revc_assets_test PRIVATE revc_core)
    target_compile_options(revc_assets_test PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(revc_assets_test PRIVATE -Wl,--gc-sections)
    add_executable(revc_gameplay_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/gameplay_test.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/../../tests/host_platform.cpp" sector.cpp gameplay.cpp)
    target_include_directories(revc_gameplay_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(revc_gameplay_test PRIVATE revc_core)
    target_compile_options(revc_gameplay_test PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(revc_gameplay_test PRIVATE -Wl,--gc-sections)
    add_executable(revc_world_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/world_test.cpp"
                   "${CMAKE_CURRENT_LIST_DIR}/../../tests/host_platform.cpp" sector.cpp gameplay.cpp world.cpp street.cpp population.cpp packed.cpp)
    target_include_directories(revc_world_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    target_link_libraries(revc_world_test PRIVATE revc_core)
    target_compile_options(revc_world_test PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(revc_world_test PRIVATE -Wl,--gc-sections)
    add_executable(revc_street_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/street_test.cpp" gameplay.cpp street.cpp)
    target_include_directories(revc_street_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_fleet_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/fleet_test.cpp" gameplay.cpp street.cpp population.cpp campaign.cpp garage.cpp vehicle_models.cpp vehicle_animation.cpp textures.cpp packed.cpp)
    target_include_directories(revc_fleet_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../generated")
    add_executable(revc_vehicle_animation_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/vehicle_animation_test.cpp" vehicle_animation.cpp animation.cpp)
    target_include_directories(revc_vehicle_animation_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_runtime_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/runtime_test.cpp" animation.cpp scripts.cpp textures.cpp water.cpp lighting.cpp population.cpp gameplay.cpp packed.cpp)
    target_include_directories(revc_runtime_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${REVC_ROOT}/src/control")
    add_executable(revc_ground_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/ground_test.cpp" gameplay.cpp world.cpp street.cpp packed.cpp)
    target_include_directories(revc_ground_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_firstmission_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/firstmission_test.cpp"
                   gameplay.cpp world.cpp street.cpp campaign.cpp packed.cpp)
    target_include_directories(revc_firstmission_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../generated")
    add_executable(revc_player_motion_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/player_motion_test.cpp" player_motion.cpp gameplay.cpp)
    target_include_directories(revc_player_motion_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../generated")
    add_executable(revc_campaign_chain_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/campaign_chain_test.cpp" gameplay.cpp world.cpp street.cpp campaign.cpp packed.cpp)
    target_include_directories(revc_campaign_chain_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../generated")
    add_executable(revc_oceanview_visibility_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/oceanview_visibility_test.cpp" gameplay.cpp world.cpp distant.cpp packed.cpp)
    target_include_directories(revc_oceanview_visibility_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../generated")
    add_executable(revc_world_streaming_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/world_streaming_test.cpp" gameplay.cpp world.cpp distant.cpp population.cpp packed.cpp)
    target_include_directories(revc_world_streaming_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    add_executable(revc_near_visibility_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/near_visibility_test.cpp" gameplay.cpp world.cpp packed.cpp)
    target_include_directories(revc_near_visibility_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../generated")
    add_executable(revc_renderer_test "${CMAKE_CURRENT_LIST_DIR}/../../tests/renderer_test.cpp" distant.cpp packed.cpp)
    target_include_directories(revc_renderer_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
    return()
endif()

add_executable(revc_dsi "${CMAKE_CURRENT_LIST_DIR}/../../source/main.cpp" sector.cpp platform.cpp gameplay.cpp world.cpp street.cpp animation.cpp scripts.cpp textures.cpp water.cpp lighting.cpp population.cpp packed.cpp campaign.cpp citymap.cpp cinematic.cpp cinematic_frame.cpp rom_storage.cpp actor_texture.cpp player_motion.cpp distant.cpp vehicle_animation.cpp frontend.cpp vehicle_models.cpp garage.cpp weapon_wheel.cpp gameplay_display.cpp)
option(REVC_GAMEPLAY_CINEMA_RESOLUTION "Present gameplay at the cutscenes' 128x72 resolution" ON)
if(REVC_GAMEPLAY_CINEMA_RESOLUTION)
    target_compile_definitions(revc_dsi PRIVATE REVC_GAMEPLAY_CINEMA_RESOLUTION)
endif()
option(REVC_INSPECT_OCEAN_VIEW "Start a development inspection build outside Ocean View" OFF)
option(REVC_INSPECT_KEN_SPAWN "Use Ken spawn with the development timing overlay" OFF)
if(REVC_INSPECT_KEN_SPAWN)
    target_compile_definitions(revc_dsi PRIVATE REVC_INSPECT_KEN_SPAWN)
endif()
if(REVC_INSPECT_OCEAN_VIEW)
    target_compile_definitions(revc_dsi PRIVATE REVC_INSPECT_OCEAN_VIEW)
endif()
target_compile_options(revc_dsi PRIVATE -O2)
target_include_directories(revc_dsi PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/../../generated")
target_include_directories(revc_dsi SYSTEM PRIVATE "${DEVKITPRO}/libnds/include" "${DEVKITPRO}/calico/include")
target_compile_definitions(revc_dsi PRIVATE REVC_SOURCE_PORT ARM9 __NDS__)
target_link_directories(revc_dsi PRIVATE "${DEVKITPRO}/libnds/lib" "${DEVKITPRO}/calico/lib")
target_link_libraries(revc_dsi PRIVATE revc_core nds9 fat calico_ds9 m)
target_link_options(revc_dsi PRIVATE "-specs=${DEVKITPRO}/calico/share/ds9.specs" "-Wl,--gc-sections,-Map,revc_dsi.map")
set_target_properties(revc_dsi PROPERTIES SUFFIX ".elf")
set(ASSET_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../sdcard/revc")
foreach(ASSET sector.vcs models/sector.dir models/sector.img world/index.vcw world/chunks.bin world/player.vcp world/index.vcf world/fast.bin world/vehicle.vcm world/guard.vcm world/pistol.vcm world/citymap.vcm world/player2.vca world/mercedes.vca)
    if(NOT EXISTS "${ASSET_ROOT}/${ASSET}")
        message(FATAL_ERROR "Missing ${ASSET}; run the preparation commands in ports/nds/README.md first")
    endif()
endforeach()
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../generated/campaign_data.h")
    message(FATAL_ERROR "Run tools/stage_navigation.py with your game directory before building")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../generated/player_motion_data.h")
    message(FATAL_ERROR "Run tools/stage_player_motion.py with your game directory before building")
endif()
if(NOT EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../generated/fleet_data.h")
    message(FATAL_ERROR "Run tools/stage_fleet.py with your game directory before building")
endif()
foreach(CAR sentinel taxi admiral spand)
    foreach(SUFFIX .vcm .vcu .vpa .atx -low.vcm -low.vcu -low.vpa)
        if(NOT EXISTS "${ASSET_ROOT}/world/fleet-${CAR}${SUFFIX}")
            message(FATAL_ERROR "Missing fleet-${CAR}${SUFFIX}; run tools/stage_fleet.py first")
        endif()
    endforeach()
endforeach()
set(ROM_ROOT "${CMAKE_CURRENT_BINARY_DIR}/romfs-world")
file(MAKE_DIRECTORY "${ROM_ROOT}/world" "${ROM_ROOT}/scripts" "${ROM_ROOT}/cutscenes")
configure_file("${ASSET_ROOT}/README.txt" "${ROM_ROOT}/README.txt" COPYONLY)
set(FRONTEND_ASSETS "${ASSET_ROOT}/cutscenes/frontend.bin" "${ASSET_ROOT}/cutscenes/logo.vcv" "${ASSET_ROOT}/cutscenes/gtatitles.vcv")
file(GLOB MISSION_MOVIES CONFIGURE_DEPENDS "${ASSET_ROOT}/cutscenes/law_*.vcv" "${ASSET_ROOT}/cutscenes/col_*.vcv" "${ASSET_ROOT}/cutscenes/tex_*.vcv")
file(GLOB FLEET_ASSETS CONFIGURE_DEPENDS "${ASSET_ROOT}/world/fleet-*")
set(VEHICLE_ASSETS ${FLEET_ASSETS} "${ASSET_ROOT}/world/vehicle.vpa" "${ASSET_ROOT}/world/traffic.vpa" "${ASSET_ROOT}/world/driver.vca")
set(CINEMATIC_GAMEPLAY_ASSETS "${ASSET_ROOT}/world/pedestrians.atx" "${ASSET_ROOT}/world/fleet-shared.atx" "${ASSET_ROOT}/world/driver2.vca" "${ASSET_ROOT}/world/driver3.vca")
foreach(ASSET IN LISTS CINEMATIC_GAMEPLAY_ASSETS)
    if(NOT EXISTS "${ASSET}")
        message(FATAL_ERROR "Missing ${ASSET}; run tools/stage_cinematic_gameplay.py with your game directory")
    endif()
endforeach()
list(APPEND VEHICLE_ASSETS ${CINEMATIC_GAMEPLAY_ASSETS})
set(ROM_ASSETS "${ASSET_ROOT}/world/collision-bounds.vcb" "${ASSET_ROOT}/world/detail.vct" ${MISSION_MOVIES} ${FRONTEND_ASSETS} ${VEHICLE_ASSETS} "${ASSET_ROOT}/world/player3.vca" "${ASSET_ROOT}/world/player3.atx" "${ASSET_ROOT}/world/averages.vcc" "${ASSET_ROOT}/world/distant.vcd" "${ASSET_ROOT}/world/vehicle-lod.vcm" "${ASSET_ROOT}/world/traffic-lod.vcm" "${ASSET_ROOT}/world/chef.vca" "${ASSET_ROOT}/world/igbuddy.vca" "${ASSET_ROOT}/world/spandxa.vca" "${ASSET_ROOT}/world/admiral.vcm" "${ASSET_ROOT}/world/spand.vcm" "${ASSET_ROOT}/world/player.atx" "${ASSET_ROOT}/world/player2.atx" "${ASSET_ROOT}/world/vehicle.atx" "${ASSET_ROOT}/world/traffic.atx" "${ASSET_ROOT}/world/vehicle.vcu" "${ASSET_ROOT}/world/traffic.vcu" "${ASSET_ROOT}/world/index.vct" "${ASSET_ROOT}/world/textured.bin" "${ASSET_ROOT}/world/textures.vct" "${ASSET_ROOT}/world/water.vcw" "${ASSET_ROOT}/world/lighting.vcl" "${ASSET_ROOT}/world/collision.vcc" "${ASSET_ROOT}/world/collision.bin" "${ASSET_ROOT}/world/backdrops.vcb" "${ASSET_ROOT}/world/terrain.vct" "${ASSET_ROOT}/world/terrain.bin" "${ASSET_ROOT}/world/paths.vcp" "${ASSET_ROOT}/world/traffic.vcm" "${ASSET_ROOT}/world/male01.vca" "${ASSET_ROOT}/world/hfyst.vca"
               "${ASSET_ROOT}/world/citymap.vcm" "${ASSET_ROOT}/world/player2.vca" "${ASSET_ROOT}/world/mercedes.vca"
               "${ASSET_ROOT}/world/player.vcp" "${ASSET_ROOT}/README.txt"
               "${ASSET_ROOT}/world/vehicle.vcm" "${ASSET_ROOT}/world/guard.vcm" "${ASSET_ROOT}/world/pistol.vcm"
               "${ASSET_ROOT}/world/player.vca" "${ASSET_ROOT}/world/guard.vca" "${ASSET_ROOT}/scripts/main.scm" "${ASSET_ROOT}/cutscenes/int_a.vcv" "${ASSET_ROOT}/cutscenes/int_m.vcv" "${ASSET_ROOT}/cutscenes/int_d.vcv" "${ASSET_ROOT}/cutscenes/int_b.vcv" "${ASSET_ROOT}/cutscenes/departure.vcv")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/revc_dsi.nds"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${FRONTEND_ASSETS} "${ROM_ROOT}/cutscenes"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ASSET_ROOT}/world/collision-bounds.vcb" "${ROM_ROOT}/world"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ASSET_ROOT}/world/detail.vct" "${ROM_ROOT}/world"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${VEHICLE_ASSETS} "${ROM_ROOT}/world"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${ASSET_ROOT}/world/index.vct" "${ASSET_ROOT}/world/textured.bin" "${ASSET_ROOT}/world/textures.vct" "${ASSET_ROOT}/world/water.vcw" "${ASSET_ROOT}/world/lighting.vcl" "${ASSET_ROOT}/world/collision.vcc" "${ASSET_ROOT}/world/collision.bin" "${ASSET_ROOT}/world/backdrops.vcb" "${ASSET_ROOT}/world/terrain.vct" "${ASSET_ROOT}/world/terrain.bin" "${ASSET_ROOT}/world/paths.vcp" "${ASSET_ROOT}/world/traffic.vcm" "${ASSET_ROOT}/world/male01.vca" "${ASSET_ROOT}/world/hfyst.vca" "${ASSET_ROOT}/world/player.vcp"
            "${ASSET_ROOT}/world/vehicle.vcm" "${ASSET_ROOT}/world/guard.vcm" "${ASSET_ROOT}/world/pistol.vcm"
            "${ASSET_ROOT}/world/player.vca" "${ASSET_ROOT}/world/guard.vca"
            "${ASSET_ROOT}/world/citymap.vcm" "${ASSET_ROOT}/world/player2.vca" "${ASSET_ROOT}/world/mercedes.vca"
            "${ASSET_ROOT}/world/player.atx" "${ASSET_ROOT}/world/player2.atx" "${ASSET_ROOT}/world/vehicle.atx" "${ASSET_ROOT}/world/traffic.atx" "${ASSET_ROOT}/world/vehicle.vcu" "${ASSET_ROOT}/world/traffic.vcu"
            "${ASSET_ROOT}/world/distant.vcd" "${ASSET_ROOT}/world/vehicle-lod.vcm" "${ASSET_ROOT}/world/traffic-lod.vcm" "${ASSET_ROOT}/world/chef.vca" "${ASSET_ROOT}/world/igbuddy.vca" "${ASSET_ROOT}/world/spandxa.vca" "${ASSET_ROOT}/world/admiral.vcm" "${ASSET_ROOT}/world/spand.vcm"
            "${ASSET_ROOT}/world/averages.vcc"
            "${ASSET_ROOT}/world/player3.vca" "${ASSET_ROOT}/world/player3.atx"
            "${ROM_ROOT}/world"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ASSET_ROOT}/scripts/main.scm" "${ROM_ROOT}/scripts"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${ASSET_ROOT}/cutscenes/int_a.vcv" "${ASSET_ROOT}/cutscenes/int_m.vcv" "${ASSET_ROOT}/cutscenes/int_d.vcv" "${ASSET_ROOT}/cutscenes/int_b.vcv" "${ASSET_ROOT}/cutscenes/departure.vcv" "${ROM_ROOT}/cutscenes"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${MISSION_MOVIES} "${ROM_ROOT}/cutscenes"
    COMMAND "${DEVKITPRO}/tools/bin/ndstool" -c "${CMAKE_CURRENT_BINARY_DIR}/revc_dsi.nds"
            -9 "$<TARGET_FILE:revc_dsi>" -7 "${DEVKITPRO}/calico/bin/ds7_maine.elf"
            -d "${ROM_ROOT}"
    DEPENDS revc_dsi ${ROM_ASSETS}
    COMMENT "Embedding bounded runtime map tiles in the reVC DSi ROM"
    VERBATIM)
add_custom_target(revc_rom ALL DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/revc_dsi.nds")

add_executable(revc_game_link_probe EXCLUDE_FROM_ALL game_link_probe.cpp platform.cpp)
target_include_directories(revc_game_link_probe SYSTEM PRIVATE "${DEVKITPRO}/libnds/include" "${DEVKITPRO}/calico/include")
target_compile_definitions(revc_game_link_probe PRIVATE ARM9 __NDS__)
target_link_directories(revc_game_link_probe PRIVATE "${DEVKITPRO}/libnds/lib" "${DEVKITPRO}/calico/lib")
target_link_libraries(revc_game_link_probe PRIVATE revc_core nds9 fat calico_ds9 m)
target_link_options(revc_game_link_probe PRIVATE "-specs=${DEVKITPRO}/calico/share/ds9.specs" "-Wl,--gc-sections")

# Opt-in timing artifact using the production player; never part of release boot.
add_executable(revc_cinematic_probe_elf EXCLUDE_FROM_ALL cinematic_probe.cpp cinematic.cpp cinematic_frame.cpp packed.cpp rom_storage.cpp)
target_include_directories(revc_cinematic_probe_elf SYSTEM PRIVATE "${DEVKITPRO}/libnds/include" "${DEVKITPRO}/calico/include")
target_compile_definitions(revc_cinematic_probe_elf PRIVATE ARM9 __NDS__)
target_link_directories(revc_cinematic_probe_elf PRIVATE "${DEVKITPRO}/libnds/lib" "${DEVKITPRO}/calico/lib")
target_link_libraries(revc_cinematic_probe_elf PRIVATE nds9 fat calico_ds9 m)
target_link_options(revc_cinematic_probe_elf PRIVATE "-specs=${DEVKITPRO}/calico/share/ds9.specs" "-Wl,--gc-sections")
set_target_properties(revc_cinematic_probe_elf PROPERTIES SUFFIX ".elf")
set(PROBE_ROOT "${CMAKE_CURRENT_BINARY_DIR}/cinematic-probe-romfs")
set(PROBE_MOVIE "${CMAKE_CURRENT_LIST_DIR}/../../build/cutscenes-motion/int_m.vcv")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/revc_cinematic_probe.nds"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROBE_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PROBE_MOVIE}" "${PROBE_ROOT}/sample.vcv"
    COMMAND "${DEVKITPRO}/tools/bin/ndstool" -c "${CMAKE_CURRENT_BINARY_DIR}/revc_cinematic_probe.nds"
        -9 "$<TARGET_FILE:revc_cinematic_probe_elf>" -7 "${DEVKITPRO}/calico/bin/ds7_maine.elf" -d "${PROBE_ROOT}"
    DEPENDS revc_cinematic_probe_elf "${PROBE_MOVIE}" VERBATIM)
add_custom_target(revc_cinematic_probe DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/revc_cinematic_probe.nds")
