cmake_minimum_required(VERSION 2.8)
project(EyeBlinkBehavour)

################################################################################
# ARDUINO BOARD 
################################################################################
find_package( PythonInterp REQUIRED )

if( NOT PORT )
execute_process( 
    COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/list_serial_ports.sh 
    OUTPUT_VARIABLE PORT 
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif(NOT PORT)

if (NOT PORT )
    message(FATAL_ERROR 
        "Could not find a serial port automatically. You won't be able to upload"
        "\n and run the arduino target. If you are sure that arduino board is "
        "\n connected and functional, you should provide port path from "
        "\n command line: e.g. \n"
        "\t cmake -DPORT=/dev/ttyACM0 .."
        "\n\n Or you may not be part of 'dailout' group?"
        )
else (NOT PORT)
    message(STATUS "Default serial port ${PORT}" )
endif (NOT PORT)


# Options related to Arduino board.
# Set the board tag
set(ARDUINO_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Extra paths to search for headers.
set(ARDUINO_EXTRA_CXXFLAGS "")

set(BOARD_TAG   uno)
set(BAUD_RATE   38400)
find_program(ARDUINO_BIN arduino )

set(ARDUINO_BOARD_FILE hardware/arduino/boards.txt)
find_path( ARDUINO_DIR 
    NAMES boards.txt
    PATHS /usr/share/arduino/hardware
    PATH_SUFFIXES arduino arduino/avr
    )
message( STATUS "Arduino location ${ARDUINO_DIR}" )
if(NOT ARDUINO_DIR)
    message(FATAL_ERROR "Arduino board list not found")
endif( )
message(STATUS "List of arduino boards ${ARDUINO_DIR}")

################################################################################
# Animal related options.
################################################################################
set(ANIMAL_NAME "test_animal")
set(SESSION_TYPE 00)
set(SESSION_NUM 00)
if( (NOT ANIMAL_NAME) OR (NOT SESSION_TYPE) OR (NOT SESSION_NUM) )
    message( FATAL_ERROR 
        "Please pass ANIMAL_NAME, SESSION_NUM and SESSION_TYPE to cmake \n"
        "\te.g cmake -DANIMAL_NAME=k3 -DSESSION_NUM=1 -DSESSION_TYPE=S .. \n"
        )
endif ( )

# Data directory for given session and animal
set(DATADIR "$ENV{HOME}/DATA/${ANIMAL_NAME}/${ANIMAL_NAME}_${SESSION_TYPE}_${SESSION_NUM}")

# Python script handle this.
# file(MAKE_DIRECTORY ${DATADIR})


configure_file( ${CMAKE_SOURCE_DIR}/Makefile.arduino.in
    ${CMAKE_SOURCE_DIR}/Makefile.arduino
    )

configure_file( ${CMAKE_SOURCE_DIR}/src/config.h.in
    ${CMAKE_SOURCE_DIR}/src/config.h
    )

set(ARDUINO_TARGET_NAME eyeBlinkBehaviour)
set(ARDUINO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(ARDDUINO_OUT_HEX_FILE 
    ${CMAKE_SOURCE_DIR}/${BOARD_NAME}/${ARDUINO_TARGET_NAME}/${ARDUINO_TARGET_NAME}.hex 
    )

add_custom_target( build_arduino_hex  ALL DEPENDS ${ARDDUINO_OUT_HEX_FILE}
    VERBATIM 
    )

add_custom_command( OUTPUT ${ARDDUINO_OUT_HEX_FILE}
    COMMAND $(MAKE) -f ${CMAKE_SOURCE_DIR}/Makefile.arduino
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
    )

add_custom_target( upload 
    DEPENDS build_arduino_hex 
    COMMAND make -f ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.arduino upload 
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

# make miniterm will launch the miniterm.py .
add_custom_target( miniterm
    DEPENDS upload 
    COMMAND miniterm.py ${PORT} ${BAUD_RATE}
    VERBATIM 
    )

# Reset all attached boards.
add_custom_target( reset_boards
    COMMAND bash scripts/reset_all_boards.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
    )

# Before run is executed, all scripts must be current binary directory.
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/run.sh.in ${CMAKE_BINARY_DIR}/run.sh )
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/copy_file.sh.in
    ${CMAKE_BINARY_DIR}/copy_file.sh )
configure_file( ${CMAKE_SOURCE_DIR}/camera_arduino_client.py.in 
    ${CMAKE_BINARY_DIR}/camera_arduino_client.py 
    )
configure_file( ${CMAKE_SOURCE_DIR}/pyblink/config.py ${CMAKE_BINARY_DIR}/config.py )

# Pass animal options to run.sh which will will passed to python script.
SET(RUN_ARGS 
    "-p ${PORT} -n ${ANIMAL_NAME} -sn ${SESSION_NUM} -st ${SESSION_TYPE}"
     )

file(COPY ${CMAKE_SOURCE_DIR}/eyeblinkdetector/extract.py 
        ${CMAKE_SOURCE_DIR}/blinky.py
        DESTINATION ${CMAKE_BINARY_DIR}
     )

add_custom_target( run  
    DEPENDS cam_server upload
    COMMAND ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_SOURCE_DIR}/pyblink ${CMAKE_BINARY_DIR}
    COMMAND bash -x ./run.sh ${RUN_ARGS}
    COMMENT "Run arduino client and camera client"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    VERBATIM
    )

set( THREADMILL_TEST_SCRIPT test_threadmill.py )
add_custom_target( threadmill
    DEPENDS upload
    COMMAND python ${THREADMILL_TEST_SCRIPT} ${PORT}
    COMMENT "Testing threadmill"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
    )


# Now  add the client and server 
add_subdirectory( ${CMAKE_SOURCE_DIR}/PointGreyCamera )

# NANA
add_definitions( -std=c++14 )
add_definitions( -DWITH_NANA )
set( NANA_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/nana )
set( NANA_INSTALL_DIR ${CMAKE_BINARY_DIR}/_nana )
set( NANA_BUILD_DIR ${CMAKE_BINARY_DIR}/_build_nana )
file(MAKE_DIRECTORY ${NANA_BUILD_DIR})

set( NANA_LIBRARIES ${NANA_INSTALL_DIR}/lib/libnana.a )
add_custom_target(nana_local DEPENDS ${NANA_LIBRARIES} )
add_custom_command( OUTPUT ${NANA_LIBRARIES}
    COMMAND ${CMAKE_COMMAND} 
            -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
            -DCMAKE_BUILD_TYPE=Release
            -DNANA_CMAKE_ENABLE_PNG=ON
            -DBUILD_SHARED_LIBS=OFF 
            -DCMAKE_INSTALL_PREFIX=${NANA_INSTALL_DIR} 
        ${NANA_SOURCE_DIR}
    COMMAND $(MAKE) 
    COMMAND $(MAKE) install
    WORKING_DIRECTORY ${NANA_BUILD_DIR}
    COMMENT "Build nana locally"
    VERBATIM
    ) 

set(NANA_INCLUDE_DIRS ${NANA_INSTALL_DIR}/include )
include_directories( ${NANA_INCLUDE_DIRS} )


# Check for pkg-config modules. We need to link with their libs.
find_package( PkgConfig REQUIRED )
set( OTHER_LIBS "" )
pkg_check_modules( LIBNOTIFY libnotify REQUIRED )
list(APPEND OTHER_LIBS ${LIBNOTIFY_LIBRARIES})
add_definitions( ${LIBNOTIFY_CFLAGS} )
pkg_check_modules( GDK2 gdk-x11-2.0 )
list(APPEND OTHER_LIBS ${GDK2_LIBRARIES} )
pkg_check_modules( X11 x11)
list(APPEND OTHER_LIBS ${X11_LIBRARIES} )
pkg_check_modules( XFT xft)
list(APPEND OTHER_LIBS ${XFT_LIBRARIES} )
pkg_check_modules( LIBPNG libpng)
list(APPEND OTHER_LIBS ${LIBPNG_LIBRARIES} )
pkg_check_modules( FONTCONFIG fontconfig)
list(APPEND OTHER_LIBS ${FONTCONFIG_LIBRARIES} )

##pkg_check_modules( XRANDR xrandr)
##list(APPEND OTHER_LIBS ${XRANDR_LIBRARIES} )

# pthreads
find_package( Threads )
list(APPEND OTHER_LIBS ${CMAKE_THREAD_LIBS_INIT} )

find_package( Boost REQUIRED COMPONENTS iostreams system filesystem date_time )
include_directories( ${Boost_INCLUDE_DIRS} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/external/boost-process/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/external/boost-asio/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/external/boost-winapi/include )


# Add executatble.
add_executable( chuha ./main.cpp )
add_dependencies( chuha nana_local )
target_link_libraries( chuha ${NANA_LIBRARIES} ${OTHER_LIBS} )

