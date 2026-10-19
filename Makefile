DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
export DEVKITPRO DEVKITARM
export PATH := $(DEVKITARM)/bin:$(DEVKITPRO)/tools/bin:$(PATH)
.DEFAULT_GOAL := revc
.PHONY: all prepare test revc revc-test
all: revc


prepare:
	python3 tools/prepare.py "$(GAME_DIR)"

test:
	python3 -m unittest discover -s tests -v

revc:
	cmake -S ports/nds -B build/revc-nds -DCMAKE_TOOLCHAIN_FILE="$(CURDIR)/cmake/nds-toolchain.cmake" -DDEVKITPRO="$(DEVKITPRO)" -DCMAKE_BUILD_TYPE=MinSizeRel
	cmake --build build/revc-nds -j4
	python3 tools/check_rom.py build/revc-nds/revc_dsi.nds --assets build/revc-nds/romfs-world

revc-test:
	cmake -S ports/nds -B build/revc-host -DREVC_NDS_HOST_VALIDATION=ON -DCMAKE_BUILD_TYPE=MinSizeRel
	cmake --build build/revc-host -j4
	python3 tools/validate_revc_loader.py "$(GAME_DIR)"
	python3 tools/validate_world.py
	python3 tools/validate_fast.py
	python3 tools/validate_quality.py
	build/revc-host/revc_gameplay_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_world_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_world_streaming_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_moving_visibility_test "$(CURDIR)/sdcard/revc"
	python3 tools/validate_spatial_streaming.py
	build/revc-host/revc_renderer_test
	build/revc-host/revc_near_visibility_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_street_test
	build/revc-host/revc_weapon_wheel_test
	build/revc-host/revc_cinematic_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_cinematic_frame_test
	build/revc-host/revc_render_quality_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_cinematic_gameplay_test "$(CURDIR)/sdcard/revc"
	python3 tools/validate_cinematics.py "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_vehicle_animation_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_fleet_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_runtime_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_player_motion_test
	build/revc-host/revc_campaign_chain_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_oceanview_visibility_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_ground_test "$(CURDIR)/sdcard/revc"
	build/revc-host/revc_firstmission_test "$(CURDIR)/sdcard/revc"
	$(CXX_HOST) -std=c++11 -Wall -Wextra -Werror -DREVC_STREAM_HOST_TEST -Iexternal/reVC/src/core tests/stream_test.cpp ports/nds/cdstream.cpp -o build/stream_test
	build/stream_test build/stream-test.img

CXX_HOST ?= g++

