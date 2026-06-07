# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

set(extra_requires)
if(DEFINED IDF_VERSION_MAJOR AND DEFINED IDF_VERSION_MINOR AND
    (IDF_VERSION_MAJOR GREATER 5 OR
     (IDF_VERSION_MAJOR EQUAL 5 AND IDF_VERSION_MINOR GREATER_EQUAL 3)))
  list(APPEND extra_requires esp_driver_tsens)
endif()

idf_component_register(SRCS ${app_sources} PRIV_REQUIRES ${extra_requires})
