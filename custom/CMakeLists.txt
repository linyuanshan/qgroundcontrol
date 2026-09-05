message(STATUS "QGC: Adding Marine Custom Plugin")

set(QGC_CLIPPER2_VERSION 2.0.1)
CPMAddPackage(
    NAME Clipper2
    VERSION ${QGC_CLIPPER2_VERSION}
    GITHUB_REPOSITORY AngusJohnson/Clipper2
    GIT_TAG Clipper2_${QGC_CLIPPER2_VERSION}
    DOWNLOAD_ONLY YES
)

if(NOT Clipper2_SOURCE_DIR)
    message(FATAL_ERROR "QGC: Clipper2 (required Marine dependency) was not resolved")
endif()

add_library(
    MarineClipper2 STATIC
    ${Clipper2_SOURCE_DIR}/CPP/Clipper2Lib/src/clipper.engine.cpp
    ${Clipper2_SOURCE_DIR}/CPP/Clipper2Lib/src/clipper.offset.cpp
    ${Clipper2_SOURCE_DIR}/CPP/Clipper2Lib/src/clipper.rectclip.cpp
    ${Clipper2_SOURCE_DIR}/CPP/Clipper2Lib/src/clipper.triangulation.cpp
)
qgc_disable_dependency_warnings(MarineClipper2)
target_compile_definitions(MarineClipper2 PUBLIC CLIPPER2_MAX_DECIMAL_PRECISION=8)
target_include_directories(MarineClipper2 SYSTEM PUBLIC ${Clipper2_SOURCE_DIR}/CPP/Clipper2Lib/include)

add_library(MarineGeometry STATIC ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Geometry/MarineGeometry.cc
                                  ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Geometry/MarineGeometry.h
)
qgc_configure_target(MarineGeometry)
target_include_directories(MarineGeometry PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine)
target_link_libraries(MarineGeometry PRIVATE MarineClipper2)

qt_add_library(MarinePlanModule STATIC)
qgc_configure_target(MarinePlanModule)

qgc_set_qt_resource_alias(${CMAKE_CURRENT_SOURCE_DIR}/src/PlanView/CoverageInspectionEditor.qml
                          ${CMAKE_CURRENT_SOURCE_DIR}/src/PlanView/CoverageInspectionMapVisual.qml
)

qt_add_qml_module(
    MarinePlanModule
    URI Marine.Plan
    VERSION 1.0
    RESOURCE_PREFIX /qml
    QML_FILES ${CMAKE_CURRENT_SOURCE_DIR}/src/PlanView/CoverageInspectionEditor.qml
              ${CMAKE_CURRENT_SOURCE_DIR}/src/PlanView/CoverageInspectionMapVisual.qml
    NO_PLUGIN
)

set(MARINE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CustomPlugin.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTask.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTask.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTaskJsonCodec.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTaskJsonCodec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/MarineTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Geometry/GeoReference.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Geometry/GeoReference.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Geometry/GeometryTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Mission/ArduPilotMissionAdapter.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Mission/ArduPilotMissionAdapter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/ICoveragePlanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/CoveragePlanningProblem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/CoverageProblemValidator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/CoverageProblemValidator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/CoverageTaskAdapter.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/CoverageTaskAdapter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/MockCoveragePlanner.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/MockCoveragePlanner.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/PlanningResult.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/PlannerRegistry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning/PlannerRegistry.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/QGC/MarinePlanContext.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/QGC/MarinePlanContext.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MissionManager/CoverageInspectionComplexItem.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MissionManager/CoverageInspectionComplexItem.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MissionManager/CoverageInspectionPlanCreator.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MissionManager/CoverageInspectionPlanCreator.h
)

if(QGC_BUILD_TESTING)
    list(
        APPEND
        MARINE_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/ArduPilotMissionAdapterTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/ArduPilotMissionAdapterTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoveragePlannerTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoveragePlannerTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoverageProblemValidatorTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoverageProblemValidatorTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoverageTaskAdapterTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoverageTaskAdapterTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/GeoReferenceTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/GeoReferenceTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/GeometryTypesTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/GeometryTypesTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarineGeometryTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarineGeometryTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoverageInspectionPlanCreatorTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoverageInspectionPlanCreatorTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoverageComplexItemTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CoverageComplexItemTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CustomPluginIntegrationTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/CustomPluginIntegrationTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarinePlanContextTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarinePlanContextTest.h
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarinePlanIntegrationTest.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/test/Marine/MarinePlanIntegrationTest.h
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
        CustomPluginIntegrationTest
        LABELS
        Unit
        MissionManager
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        CoverageInspectionPlanCreatorTest
        LABELS
        Unit
        MissionManager
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        ArduPilotMissionAdapterTest
        LABELS
        Unit
        MissionManager
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        CoverageComplexItemTest
        LABELS
        Unit
        MissionManager
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
        CoverageProblemValidatorTest
        LABELS
        Unit
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        CoverageTaskAdapterTest
        LABELS
        Unit
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        GeoReferenceTest
        LABELS
        Unit
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        GeometryTypesTest
        LABELS
        Unit
    )

    cmake_language(
        DEFER
        DIRECTORY
        "${CMAKE_SOURCE_DIR}"
        CALL
        add_qgc_test
        MarineGeometryTest
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
        MarinePlanIntegrationTest
        LABELS
        Integration
        MissionManager
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Mission ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/Planning
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Marine/QGC ${CMAKE_CURRENT_SOURCE_DIR}/src/MissionManager
    PARENT_SCOPE
)

set(CUSTOM_LIBRARIES
    MarineGeometry MarinePlanModule
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
