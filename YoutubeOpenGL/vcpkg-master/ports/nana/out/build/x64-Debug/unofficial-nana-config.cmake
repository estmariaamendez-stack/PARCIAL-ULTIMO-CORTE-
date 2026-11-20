
####### Expanded from @PACKAGE_INIT@ by configure_package_config_file() #######
####### Any changes to this file will be overwritten by the next CMake run ####
####### The input file was config.cmake.in                            ########

get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

macro(set_and_check _var _file)
  set(${_var} "${_file}")
  if(NOT EXISTS "${_file}")
    message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
  endif()
endmacro()

macro(check_required_components _NAME)
  foreach(comp ${${_NAME}_FIND_COMPONENTS})
    if(NOT ${_NAME}_${comp}_FOUND)
      if(${_NAME}_FIND_REQUIRED_${comp})
        set(${_NAME}_FOUND FALSE)
      endif()
    endif()
  endforeach()
endmacro()

####################################################################################
include(CMakeFindDependencyMacro)

if(UNIX)
  find_dependency(Threads)
  find_dependency(Freetype)
  find_dependency(X11)
  find_dependency(Fontconfig)
endif()

option(NANA_ENABLE_PNG "Enable PNG support" OFF)
option(NANA_ENABLE_JPEG "Enable JPEG support" OFF)

if(NANA_ENABLE_PNG)
  find_dependency(PNG)
endif()

if(NANA_ENABLE_JPEG)
  find_dependency(JPEG)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/unofficial-nana-targets.cmake")

if(UNIX)
  target_link_libraries(unofficial::nana::nana INTERFACE Fontconfig::Fontconfig ${X11_LIBRARIES} ${X11_Xft_LIB})
endif()

if(NANA_ENABLE_JPEG)
  target_link_libraries(unofficial::nana::nana INTERFACE ${JPEG_LIBRARIES})
endif()
