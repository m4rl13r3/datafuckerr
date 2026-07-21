cmake_minimum_required(VERSION 3.20)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" DISKPURGE_VERSION LIMIT_COUNT 1)
string(STRIP "${DISKPURGE_VERSION}" DISKPURGE_VERSION)
if(NOT DISKPURGE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION doit contenir une version sémantique comme 0.2.0")
endif()

project(diskpurge VERSION "${DISKPURGE_VERSION}" LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(DISKPURGE_ENABLE_SANITIZERS "Active AddressSanitizer et UndefinedBehaviorSanitizer" OFF)
option(DISKPURGE_ENABLE_LAB_MODE "Compile le mode de laboratoire réservé aux tests matériels" OFF)
option(DISKPURGE_ENABLE_FUZZING "Compile les cibles libFuzzer non destructives" OFF)

function(diskpurge_configure_target target)
    target_compile_definitions(${target} PRIVATE DFX_VERSION="${PROJECT_VERSION}")
    if(DISKPURGE_ENABLE_LAB_MODE)
        target_compile_definitions(${target} PRIVATE DFX_ENABLE_LAB_MODE=1)
    endif()
    if(MSVC)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    if(DISKPURGE_ENABLE_SANITIZERS)
        if(MSVC)
            message(FATAL_ERROR "DISKPURGE_ENABLE_SANITIZERS exige Clang ou GCC")
        endif()
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    endif()
endfunction()

add_executable(diskpurge
    src/main.c
    src/audit.c
    src/device.c
    src/erase.c
    src/purge.c
    src/platform.c
    src/qualification.c
    src/runtime.c
    src/sha256.c
)

target_include_directories(diskpurge PRIVATE include)
diskpurge_configure_target(diskpurge)

if(APPLE)
    find_library(IOKIT_FRAMEWORK IOKit REQUIRED)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(diskpurge PRIVATE ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
elseif(WIN32)
    target_link_libraries(diskpurge PRIVATE bcrypt)
endif()

add_executable(diskpurge_test_sha256 tests/test_sha256.c src/sha256.c)
target_include_directories(diskpurge_test_sha256 PRIVATE include)
diskpurge_configure_target(diskpurge_test_sha256)

add_executable(diskpurge_test_purge tests/test_purge.c src/purge.c src/runtime.c)
target_include_directories(diskpurge_test_purge PRIVATE include src)
diskpurge_configure_target(diskpurge_test_purge)

add_executable(diskpurge_test_safety tests/test_safety.c src/device.c src/purge.c src/qualification.c src/runtime.c)
target_include_directories(diskpurge_test_safety PRIVATE include src)
diskpurge_configure_target(diskpurge_test_safety)

add_executable(diskpurge_test_qualification tests/test_qualification.c src/qualification.c)
target_include_directories(diskpurge_test_qualification PRIVATE include)
target_compile_definitions(diskpurge_test_qualification PRIVATE DFX_TEST_QUALIFICATION=1)
diskpurge_configure_target(diskpurge_test_qualification)

add_executable(diskpurge_test_audit tests/test_audit.c src/audit.c src/device.c src/purge.c src/qualification.c src/runtime.c src/sha256.c)
target_include_directories(diskpurge_test_audit PRIVATE include src)
diskpurge_configure_target(diskpurge_test_audit)
if(WIN32)
    target_link_libraries(diskpurge_test_audit PRIVATE bcrypt)
endif()

add_executable(diskpurge_test_erase tests/test_erase.c src/erase.c src/device.c src/purge.c src/platform.c src/qualification.c src/runtime.c src/sha256.c)
target_include_directories(diskpurge_test_erase PRIVATE include src)
diskpurge_configure_target(diskpurge_test_erase)
if(APPLE)
    target_link_libraries(diskpurge_test_erase PRIVATE ${IOKIT_FRAMEWORK} ${COREFOUNDATION_FRAMEWORK})
endif()

enable_testing()
set(DISKPURGE_TEST_VERSION "${PROJECT_VERSION}")
if(DISKPURGE_ENABLE_LAB_MODE)
    string(APPEND DISKPURGE_TEST_VERSION "-lab")
endif()
if(WIN32 AND NOT CMAKE_CROSSCOMPILING)
    find_program(POWERSHELL_EXECUTABLE NAMES pwsh powershell REQUIRED)
    add_test(
        NAME cli
        COMMAND "${POWERSHELL_EXECUTABLE}" -NoLogo -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.ps1"
                $<TARGET_FILE:diskpurge>
                "${DISKPURGE_TEST_VERSION}"
    )
elseif(NOT WIN32)
    add_test(NAME cli COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cli.sh" $<TARGET_FILE:diskpurge> "${DISKPURGE_TEST_VERSION}")
endif()
add_test(NAME sha256 COMMAND diskpurge_test_sha256)
add_test(NAME nvme-virtuel COMMAND diskpurge_test_purge)
add_test(NAME surete COMMAND diskpurge_test_safety)
add_test(NAME qualification COMMAND diskpurge_test_qualification)
add_test(NAME audit COMMAND diskpurge_test_audit)
add_test(NAME effacement COMMAND diskpurge_test_erase)
set_tests_properties(sha256 nvme-virtuel surete qualification audit effacement PROPERTIES TIMEOUT 60)
if(TEST cli)
    set_tests_properties(cli PROPERTIES TIMEOUT 60)
endif()

install(TARGETS diskpurge RUNTIME DESTINATION bin)

if(DISKPURGE_ENABLE_FUZZING)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "DISKPURGE_ENABLE_FUZZING exige Clang avec libFuzzer")
    endif()
    add_executable(diskpurge_fuzz_audit
        fuzz/fuzz_audit_record.c
        src/audit.c
        src/device.c
        src/purge.c
        src/qualification.c
        src/runtime.c
        src/sha256.c
    )
    target_include_directories(diskpurge_fuzz_audit PRIVATE include src)
    diskpurge_configure_target(diskpurge_fuzz_audit)
    target_compile_options(diskpurge_fuzz_audit PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(diskpurge_fuzz_audit PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    if(WIN32)
        target_link_libraries(diskpurge_fuzz_audit PRIVATE bcrypt)
    endif()

    add_executable(diskpurge_fuzz_nvme
        fuzz/fuzz_nvme_identify.c
        src/purge.c
        src/runtime.c
    )
    target_include_directories(diskpurge_fuzz_nvme PRIVATE include src)
    diskpurge_configure_target(diskpurge_fuzz_nvme)
    target_compile_options(diskpurge_fuzz_nvme PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_link_options(diskpurge_fuzz_nvme PRIVATE -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
endif()
