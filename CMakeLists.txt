cmake_minimum_required(VERSION 3.12)

project(allocator LANGUAGES CXX)

option(WITH_BOOST_TEST "Build Boost test" ON)

# --- бинарник ---
add_executable(allocator_cli allocator.cpp)
set_target_properties(allocator_cli PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
if (MSVC)
    target_compile_options(allocator_cli PRIVATE /W4)
else()
    target_compile_options(allocator_cli PRIVATE
        -Wall -Wextra -pedantic -Werror
        -Wno-error=unused-parameter
    )
endif()

# Куда класть результаты сборки
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

install(TARGETS allocator_cli RUNTIME DESTINATION bin)

# --- тесты (Boost.Test) ---
if (WITH_BOOST_TEST)
    # На новых CMake подавим предупреждение про FindBoost
    if (POLICY CMP0167)
        cmake_policy(SET CMP0167 NEW)
    endif()

    find_package(Boost 1.70 REQUIRED COMPONENTS unit_test_framework)

    add_executable(test_version test_version.cpp)
    set_target_properties(test_version PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    # Линкуем так, чтобы покрыть и старый FindBoost, и новый BoostConfig
    if (TARGET Boost::unit_test_framework)
        target_link_libraries(test_version PRIVATE Boost::unit_test_framework)
    else()
        target_link_libraries(test_version PRIVATE ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY})
        target_include_directories(test_version PRIVATE ${Boost_INCLUDE_DIRS})
    endif()

    target_compile_definitions(test_version PRIVATE BOOST_TEST_DYN_LINK)

    if (MSVC)
        target_compile_options(test_version PRIVATE /W4)
    else()
        target_compile_options(test_version PRIVATE
            -Wall -Wextra -pedantic -Werror
            -Wno-error=unused-parameter
        )
    endif()

    enable_testing()
    add_test(NAME test_version COMMAND test_version)
endif()

# --- упаковка в .deb ---
set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_CONTACT example@example.com)
set(CPACK_PACKAGE_FILE_NAME "allocator-1.0.0-Linux")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}")
include(CPack)
