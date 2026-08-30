message(STATUS "QGC: Adding Marine Custom Plugin")

set(MARINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTask.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTask.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTaskJsonCodec.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTaskJsonCodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/ICoveragePlanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/MockCoveragePlanner.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/MockCoveragePlanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/PlannerRegistry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/PlannerRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/QGC/MarinePlanContext.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/QGC/MarinePlanContext.h
)

if(QGC_BUILD_TESTING)
    list(
        APPEND
        MARINE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoveragePlannerTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoveragePlannerTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarinePlanContextTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarinePlanContextTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarineTaskJsonTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarineTaskJsonTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarineTaskModelTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarineTaskModelTest.h
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        CoveragePlannerTest
        LABELS
        Unit
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        MarinePlanContextTest
        LABELS
        Unit
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        MarineTaskModelTest
        LABELS
        Unit
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        MarineTaskJsonTest
        LABELS
        Unit
    )
endif()

set(CUSTOM_SOURCES
    ${MARINE_SOURCES}
    PARENT_SCOPE
)

set(CUSTOM_INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/QGC
    PARENT_SCOPE
)

set(CUSTOM_DEFINITIONS
    QGC_CUSTOM_BUILD CUSTOMHEADER="CustomPlugin.h" CUSTOMCLASS=CustomPlugin
    PARENT_SCOPE
)

set(CUSTOM_QT_COMPONENTS
    Core
    PARENT_SCOPE
)
