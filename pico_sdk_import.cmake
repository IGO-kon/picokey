if (DEFINED ENV{PICO_SDK_PATH} AND (NOT PICO_SDK_PATH))
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

set(PICO_SDK_PATH "${PICO_SDK_PATH}" CACHE PATH "Path to the Raspberry Pi Pico SDK")
set(PICO_SDK_FETCH_FROM_GIT ON CACHE BOOL "Fetch the Pico SDK from Git if needed")

if (NOT PICO_SDK_PATH)
    include(FetchContent)

    FetchContent_Declare(
        pico_sdk
        GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk.git
        GIT_TAG master
        GIT_SUBMODULES_RECURSE TRUE
    )

    FetchContent_GetProperties(pico_sdk)
    if (NOT pico_sdk_POPULATED)
        message(STATUS "Fetching pico-sdk from GitHub")
        FetchContent_Populate(pico_sdk)
    endif()

    set(PICO_SDK_PATH "${pico_sdk_SOURCE_DIR}" CACHE PATH "Path to the Raspberry Pi Pico SDK" FORCE)
endif()

if (NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "Could not find external/pico_sdk_import.cmake under PICO_SDK_PATH='${PICO_SDK_PATH}'")
endif()

include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")