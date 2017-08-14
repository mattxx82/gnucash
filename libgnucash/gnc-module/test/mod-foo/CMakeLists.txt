
IF (BUILDING_FROM_VCS)
  SET (SWIG_FOO_C ${CMAKE_CURRENT_BINARY_DIR}/swig-foo.c)
  GNC_ADD_SWIG_COMMAND (swig-foo-c ${SWIG_FOO_C} ${CMAKE_CURRENT_SOURCE_DIR}/foo.i foo.h)
ELSE()
  SET (SWIG_FOO_C ${CMAKE_CURRENT_SOURCE_DIR}/swig-foo.c)
ENDIF()

ADD_LIBRARY(foo EXCLUDE_FROM_ALL foo.c foo.h)
ADD_LIBRARY(gncmodfoo EXCLUDE_FROM_ALL gnc-mod-foo.c)
SET_SOURCE_FILES_PROPERTIES (gnc-mod-foo.c PROPERTY OBJECT_DEPENDS ${SWIG_FOO_C})

TARGET_LINK_LIBRARIES(gncmodfoo foo gnc-module)
TARGET_INCLUDE_DIRECTORIES(gncmodfoo PRIVATE
  ${CMAKE_BINARY_DIR}/common
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_SOURCE_DIR}/libgnucash/gnc-module
  ${GLIB2_INCLUDE_DIRS}
  ${GUILE_INCLUDE_DIRS}
)

SET_TARGET_PROPERTIES(foo gncmodfoo PROPERTIES
LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/gnucash/test
ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/gnucash/test
RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

SET_DIST_LIST(mod_foo_DIST CMakeLists.txt Makefile.am foo.i gnucash/foo.scm foo.c foo.h gnc-mod-foo.c)