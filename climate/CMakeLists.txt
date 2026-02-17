SET(GLD_MODULE_NAME climate)

add_library(${GLD_MODULE_NAME} SHARED
        climate.cpp
        climate.h
        csv_reader.cpp
        csv_reader.h
        #init.cpp
        #main.cpp
        solar_angles.cpp
        solar_angles.h
        test.cpp
        test.h
        weather.cpp
        weather.h
        weather_reader.cpp
        weather_reader.h
)

target_compile_options(${GLD_MODULE_NAME} PRIVATE ${GLD_COMPILE_OPTIONS})
set_target_properties(${GLD_MODULE_NAME} PROPERTIES PREFIX "")

target_include_directories(${GLD_MODULE_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/gldcore"
    "${CMAKE_CURRENT_BINARY_DIR}/headers"
    "${CMAKE_BINARY_DIR}/headers"
)

# Do NOT link modules against the gridlabd executable.
# Link against the reusable core library + OS libs.
target_link_libraries(${GLD_MODULE_NAME} PRIVATE gldapi ${OS_SPECIFIC_LIBRARIES})

if(APPLE)
    # Prefer plugin-style late symbol binding on macOS
    target_link_options(${GLD_MODULE_NAME} PRIVATE -Wl,-undefined,dynamic_lookup)
endif()



if (GLD_USE_CLANG_TIDY)
    set_target_properties(${GLD_MODULE_NAME} PROPERTIES
            CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
    )
endif ()
