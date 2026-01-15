SET(GLD_MODULE_NAME commercial)

add_library(${GLD_MODULE_NAME} SHARED
        hvac.cpp
        hvac.h
        init.cpp
        main.cpp
        multizone.cpp
        multizone.h
        office.cpp
        office.h
        solvers.cpp
        solvers.h
)

target_compile_options(${GLD_MODULE_NAME} PRIVATE ${GLD_COMPILE_OPTIONS})
set_target_properties(${GLD_MODULE_NAME} PROPERTIES PREFIX "")

target_include_directories(${GLD_MODULE_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/gldcore"
    "${CMAKE_CURRENT_BINARY_DIR}/headers"
    "${CMAKE_BINARY_DIR}/headers"
)

# Link to the reusable core (not the gridlabd executable) + OS libs
target_link_libraries(${GLD_MODULE_NAME} PRIVATE gldapi ${OS_SPECIFIC_LIBRARIES})

if(APPLE)
    # plugin-style late binding on macOS
    target_link_options(${GLD_MODULE_NAME} PRIVATE -Wl,-undefined,dynamic_lookup)
endif()

if (GLD_USE_CLANG_TIDY)
    set_target_properties(${GLD_MODULE_NAME} PROPERTIES
            CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
    )
endif ()
