# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set( PYTHON_DEPS_CHECKED 1 )
set( ESP_PLATFORM 1 )
set( IDF_TARGET esp32 )
set( CCACHE_ENABLE 0 )
set( MCUBOOT_IMAGE 1 )

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(app)
