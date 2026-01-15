SET(GLD_MODULE_NAME residential)

add_library(${GLD_MODULE_NAME} SHARED
        appliance.cpp
        appliance.h
        clotheswasher.cpp
        clotheswasher.h
        dishwasher.cpp
        dishwasher.h
        dryer.cpp
        dryer.h
        evcharger.cpp
        evcharger_det.cpp
        evcharger_det.h
        evcharger.h
        freezer.cpp
        freezer.h
        house_e.cpp
        house_e.h
        init.cpp
        lights.cpp
        lights.h
        main.cpp
        microwave.cpp
        microwave.h
        occupantload.cpp
        occupantload.h
        plugload.cpp
        plugload.h
        range.cpp
        range.h
        refrigerator.cpp
        refrigerator.h
        residential_enduse.cpp
        residential_enduse.h
        residential.h
        rbsa2014.h
        solvers.cpp
        solvers.h
        thermal_storage.cpp
        thermal_storage.h
        waterheater.cpp
        waterheater.h
        zipload.cpp
        zipload.h
        elcap1990.h
        elcap2010.h
)

target_compile_options(${GLD_MODULE_NAME} PRIVATE ${GLD_COMPILE_OPTIONS})
set_target_properties(${GLD_MODULE_NAME} PROPERTIES PREFIX "")

# Core headers + generated headers
target_include_directories(${GLD_MODULE_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/gldcore"
    "${CMAKE_CURRENT_BINARY_DIR}/headers"
    "${CMAKE_BINARY_DIR}/headers"
)

# IMPORTANT:
# Do NOT link modules against the gridlabd executable (causes duplicate globals/callback issues).
# Link against the reusable core library instead.
target_link_libraries(${GLD_MODULE_NAME} PRIVATE gldapi ${OS_SPECIFIC_LIBRARIES})

# Platform specifics
if(APPLE)
    # Typical plugin behavior: allow unresolved symbols to resolve from host at load time.
    # Safe even if the module already links to gldapi, and avoids hard failures for late-bound symbols.
    target_link_options(${GLD_MODULE_NAME} PRIVATE -Wl,-undefined,dynamic_lookup)
elseif(WIN32 OR MSYS OR MINGW OR CYGWIN)
    # Nothing special required here beyond linking to gldapi/OS libs.
    # (Keep empty to preserve structure for future Windows-specific flags.)
endif()

if (GLD_USE_CLANG_TIDY)
    set_target_properties(${GLD_MODULE_NAME} PROPERTIES
        CXX_CLANG_TIDY "${CMAKE_CXX_CLANG_TIDY}"
    )
endif ()
