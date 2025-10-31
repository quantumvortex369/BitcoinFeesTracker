cmake_minimum_required(VERSION 3.10)
project(GasFeeTracker VERSION 1.0.0 LANGUAGES C)

# Configuración básica
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Opciones de compilación
option(ENABLE_DEBUG "Habilitar modo depuración" ON)
option(ENABLE_GTK "Habilitar soporte para GTK" ON)

if(ENABLE_DEBUG)
    add_compile_options(-g -O0 -Wall -Wextra -DDEBUG)
else()
    add_compile_options(-O2)
endif()

# Buscar dependencias
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(CJSON REQUIRED cJSON)

# Configuración de directorios
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${GTK3_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

link_directories(
    ${GTK3_LIBRARY_DIRS}
    ${CURL_LIBRARY_DIRS}
    ${CJSON_LIBRARY_DIRS}
)

# Fuentes
set(SOURCES
    src/btc_fee_gui.c
    src/btc_fee_visualizer.c
    src/chart_utils.c
    src/ui_utils.c
)

# Ejecutable principal
add_executable(gas-fee-tracker ${SOURCES})
target_link_libraries(gas-fee-tracker
    ${GTK3_LIBRARIES}
    ${CURL_LIBRARIES}
    ${CJSON_LIBRARIES}
    m
)

# Instalación
install(TARGETS gas-fee-tracker
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY resources/ DESTINATION share/gas-fee-tracker)

# Documentación
find_package(Doxygen)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                 ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generando documentación con Doxygen" VERBATIM
    )
endif()
