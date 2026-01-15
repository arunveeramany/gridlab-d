SET(GLD_MODULE_NAME generators)

add_library(${GLD_MODULE_NAME} SHARED
        battery.cpp
        battery.h
        central_dg_control.cpp
        central_dg_control.h
        controller_dg.cpp
        controller_dg.h
        diesel_dg.cpp
        diesel_dg.h
        energy_storage.cpp
        energy_storage.h
        generators.h
        init.cpp
        inverter.cpp
        inverter.h
        inverter_dyn.cpp
        inverter_dyn.h
        main.cpp
        rectifier.cpp
        rectifier.h
        sec_control.cpp
        sec_control.h
        solar.cpp
        solar.h
        sync_ctrl.cpp
        sync_ctrl.h
        windturb_dg.cpp
        windturb_dg.h
        cblock.cpp
        cblock.h
)

target_compile_options(${GLD_MODULE_NAME} PRIVATE ${GLD_COMPILE_OPTIONS})
set_target_properties(${GLD_MODULE_NAME} PROPERTIES PREFIX "")

target_include_directories(${GLD_MODULE_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/gldcore"
    "${CMAKE_CURRENT_BINARY_DIR}/headers"
    "${CMAKE_BINARY_DIR}/headers"
)

# Link against the reusable core library (not the gridlabd executable) + OS libs
target_link_libraries(${GLD_MODULE_NAME} PRIVATE gldapi ${OS_SPECIFIC_LIBRARIES})

if (GLD_USE_EIGEN)
    target_include_directories(${GLD_MODULE_NAME} PRIVATE
            "${CMAKE_SOURCE_DIR}/third_party/Eigen"
    )
    target_link_libraries(${GLD_MODULE_NAME} PRIVATE
            Eigen3::Eigen
    )
else ()
    target_link_libraries(${GLD_MODULE_NAME} PRIVATE
            ${SUPERLU_LIB}
    )
endif ()

if(APPLE)
    # Useful for plugin-style modules on macOS (late-bound symbol resolution)
    target_link_options(${GLD_MODULE_NAME} PRIVATE -Wl,-undefined,dynamic_lookup)
endif()

if (GLD_USE_CLANG_TIDY)
    set_target_properties(${GLD_MODULE_NAME} PROPERTIES
            CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
    )
endif ()
