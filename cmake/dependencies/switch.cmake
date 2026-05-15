#
# Switch (Nintendo Switch Homebrew) platform dependencies
#

message(STATUS "Configuring dependencies for Nintendo Switch")

include(FetchContent)

# Ensure Switch portlibs CMake packages are discoverable.
list(APPEND CMAKE_PREFIX_PATH "/opt/devkitpro/portlibs/switch/lib/cmake")

# Use OpenGL ES on Switch.
set(USE_OPENGLES ON CACHE BOOL "" FORCE)
# Ensure ImGui OpenGL backend compiles as GLES on Switch and doesn't pull desktop GL loader paths.
add_compile_definitions(IMGUI_IMPL_OPENGL_ES3)

# No libusb on Switch.
set(HIDAPI_WITH_LIBUSB OFF CACHE BOOL "" FORCE)
set(HIDAPI_WITH_HIDRAW OFF CACHE BOOL "" FORCE)

# Prefer static linking in the NRO.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Available package configs in devkitPro switch portlibs.
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

#=================== zlib ===================
find_package(ZLIB QUIET)
if (NOT ${ZLIB_FOUND})
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(
		ZLIB
		GIT_REPOSITORY https://github.com/madler/zlib.git
		GIT_TAG v1.3.1
		OVERRIDE_FIND_PACKAGE
	)
	FetchContent_MakeAvailable(ZLIB)
endif()

if (NOT TARGET ZLIB::ZLIB)
	if (TARGET zlibstatic)
		add_library(ZLIB::ZLIB ALIAS zlibstatic)
	elseif(TARGET zlib)
		add_library(ZLIB::ZLIB ALIAS zlib)
	endif()
endif()

#=================== libzip ===================
find_package(libzip QUIET)
if (NOT ${libzip_FOUND})
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(BUILD_TOOLS OFF)
	set(BUILD_REGRESS OFF)
	set(BUILD_EXAMPLES OFF)
	set(BUILD_DOC OFF)
	set(BUILD_OSSFUZZ OFF)
	set(LIBZIP_DO_INSTALL OFF CACHE BOOL "" FORCE)
	set(BUILD_SHARED_LIBS OFF)
	FetchContent_Declare(
		libzip
		GIT_REPOSITORY https://github.com/nih-at/libzip.git
		GIT_TAG v1.11.4
		OVERRIDE_FIND_PACKAGE
	)
	FetchContent_MakeAvailable(libzip)
endif()

if (NOT TARGET libzip::zip AND TARGET zip)
	add_library(libzip::zip ALIAS zip)
endif()

#=================== nlohmann-json ===================
find_package(nlohmann_json QUIET)
if (NOT ${nlohmann_json_FOUND})
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	FetchContent_Declare(
		nlohmann_json
		GIT_REPOSITORY https://github.com/nlohmann/json.git
		GIT_TAG v3.12.0
		OVERRIDE_FIND_PACKAGE
	)
	FetchContent_MakeAvailable(nlohmann_json)
endif()

if (NOT TARGET nlohmann_json::nlohmann_json AND TARGET nlohmann_json)
	add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

#=================== tinyxml2 ===================
find_package(tinyxml2 QUIET)
if (NOT ${tinyxml2_FOUND})
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	set(tinyxml2_BUILD_TESTING OFF)
	FetchContent_Declare(
		tinyxml2
		GIT_REPOSITORY https://github.com/leethomason/tinyxml2.git
		GIT_TAG 11.0.0
		OVERRIDE_FIND_PACKAGE
	)
	FetchContent_MakeAvailable(tinyxml2)
endif()

if (NOT TARGET tinyxml2::tinyxml2 AND TARGET tinyxml2)
	add_library(tinyxml2::tinyxml2 ALIAS tinyxml2)
elseif (NOT TARGET tinyxml2::tinyxml2 AND TARGET tinyxml2_static)
	add_library(tinyxml2::tinyxml2 ALIAS tinyxml2_static)
endif()

#=================== spdlog ===================
find_package(spdlog QUIET)
if (NOT ${spdlog_FOUND})
	set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
	# newlib on Switch lacks some unlocked stdio APIs expected by upstream defaults.
	set(SPDLOG_FWRITE_UNLOCKED OFF CACHE BOOL "" FORCE)
	FetchContent_Declare(
		spdlog
		GIT_REPOSITORY https://github.com/gabime/spdlog.git
		GIT_TAG v1.16.0
		OVERRIDE_FIND_PACKAGE
	)
	FetchContent_MakeAvailable(spdlog)
endif()

if (TARGET spdlog)
	target_compile_definitions(spdlog PUBLIC _POSIX_C_SOURCE=200809L)
endif()

if (TARGET spdlog_header_only)
	target_compile_definitions(spdlog_header_only INTERFACE _POSIX_C_SOURCE=200809L)
endif()
