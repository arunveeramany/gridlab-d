SET(GLD_MODULE_NAME market)

add_library(${GLD_MODULE_NAME} SHARED
        auction.cpp
        auction.h
        bid.cpp
        bid.h
        collect.cpp
        collect.h
        controller.cpp
        controller.h
        curve.cpp
        curve.h
        double_controller.cpp
        double_controller.h
        generator_controller.cpp
        generator_controller.h
        init.cpp
        main.cpp
        market.h
        passive_controller.cpp
        passive_controller.h
        stub_bidder.cpp
        stub_bidder.h
        stubauction.cpp
        stubauction.h
        supervisory_control.cpp
        supervisory_control.h
)

target_compile_options(${GLD_MODULE_NAME} PRIVATE ${GLD_COMPILE_OPTIONS})
set_target_properties(${GLD_MODULE_NAME} PROPERTIES PREFIX "")

target_include_directories(${GLD_MODULE_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/gldcore"
    "${CMAKE_CURRENT_BINARY_DIR}/headers"
    "${CMAKE_BINARY_DIR}/headers"
)

# Link to the reusable core (not the gridlabd executable) + platform libs
target_link_libraries(${GLD_MODULE_NAME} PRIVATE gldapi ${OS_SPECIFIC_LIBRARIES})

if(APPLE)
    # Helpful for plugin-style modules on macOS (late-bound symbol resolution)
    target_link_options(${GLD_MODULE_NAME} PRIVATE -Wl,-undefined,dynamic_lookup)
endif()

if (GLD_USE_CLANG_TIDY)
    set_target_properties(${GLD_MODULE_NAME} PROPERTIES
            CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
    )
endif ()