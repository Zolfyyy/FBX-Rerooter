set(SOURCES
    main.cpp
    fbx_helpers.cpp
)

set(PUBLIC_HEADERS
    # Add public headers here as they are created
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${PUBLIC_HEADERS}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX AND NOT APPLE)
    # Linux-specific settings
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PROJECT_VERSION="${PROJECT_VERSION}"
)

# ── Link dependencies ─────────────────────────────────────────────────────────
target_link_libraries(${PROJECT_NAME} PRIVATE CLI11::CLI11 FBX::SDK)

# ── Install target ───────────────────────────────────────────────────────────
install(
    TARGETS     ${PROJECT_NAME}
    EXPORT      ${PROJECT_NAME}Targets
    RUNTIME     COMPONENT ${PROJECT_NAME}_Runtime
    LIBRARY     COMPONENT ${PROJECT_NAME}_Runtime
    ARCHIVE     COMPONENT ${PROJECT_NAME}_Development
)

install(
    FILES       ${PUBLIC_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
)