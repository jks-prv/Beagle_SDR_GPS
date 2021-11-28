include(CheckCCompilerFlag)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckCSourceRuns)
include(TestBigEndian)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMakeModules")
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# strdup, pthread_barriers
add_definitions(-D_XOPEN_SOURCE=600)

# strsep
if(APPLE)
	add_definitions(-D_DARWIN_C_SOURCE)
elseif(UNIX)
	add_definitions(-D_DEFAULT_SOURCE)
endif()

if(UNIX OR MINGW)
	add_definitions(-D_FILE_OFFSET_BITS=64)
endif()

find_library(LIBM m REQUIRED)

find_library(LIBPTHREAD pthread REQUIRED)
CHECK_C_COMPILER_FLAG(-pthread CC_HAS_PTHREAD)
if(CC_HAS_PTHREAD)
	add_definitions(-pthread)
endif()

CHECK_C_COMPILER_FLAG(-ffast-math CC_HAS_FFAST_MATH)
if(CC_HAS_FFAST_MATH)
	add_definitions(-ffast-math)
endif()

CHECK_C_COMPILER_FLAG(-fno-common CC_HAS_FNO_COMMON)
if(CC_HAS_FNO_COMMON)
	add_definitions(-fno-common)
endif()

# Kiwi
if(TRUE)
	set(HAVE_PTHREAD_BARRIERS FALSE)
	list(APPEND dumphfdl_extra_sources pthr.c)
	list(APPEND dumphfdl_extra_sources pthread_barrier.c)
else()
set(CMAKE_REQUIRED_FLAGS_ORIG ${CMAKE_REQUIRED_FLAGS})
endif()
if(CC_HAS_PTHREAD)
	set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -pthread")
endif()
CHECK_SYMBOL_EXISTS(pthread_barrier_wait pthread.h HAVE_PTHREAD_BARRIERS)
if(NOT HAVE_PTHREAD_BARRIERS)
	list(APPEND dumphfdl_extra_sources pthread_barrier.c)
endif()
set(CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_ORIG})

if(DATADUMPS)
	list(APPEND dumphfdl_extra_sources dumpfile.c)
endif()

find_package(PkgConfig REQUIRED)

find_package(LiquidDSP REQUIRED)
set(CMAKE_REQUIRED_INCLUDES_SAVE ${CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${LIQUIDDSP_INCLUDE_DIR})
set(CMAKE_REQUIRED_LIBRARIES_SAVE ${CMAKE_REQUIRED_LIBRARIES})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${LIQUIDDSP_LIBRARIES})
set(LIQUID_VERSION_MIN "1.3.0")
set(LIQUID_VERSION_NUMBER_MIN 1003000)
check_c_source_runs("
#include <stdio.h>
#include <stdlib.h>
#include <liquid/liquid.h>
#if LIQUID_VERSION_NUMBER < ${LIQUID_VERSION_NUMBER_MIN}
#error LiquidDSP library is too old
#endif
int main(void) { LIQUID_VALIDATE_LIBVERSION return 0; }" LIQUIDDSP_VERSION_CHECK)
if(LIQUIDDSP_VERSION_CHECK)
	list(APPEND dumphfdl_extra_libs ${LIQUIDDSP_LIBRARIES})
	list(APPEND dumphfdl_include_dirs ${LIQUIDDSP_INCLUDE_DIR})
else()
	message(FATAL_ERROR "LiquidDSP library is too old or missing (min. version required: ${LIQUID_VERSION_MIN})")
endif()
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES_SAVE})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES_SAVE})

set(FFT_LIBRARY "fftw" CACHE STRING "Specify which library shall be used for FFT computation")
if(FFT_LIBRARY STREQUAL "fftw")
	pkg_check_modules(FFTW REQUIRED fftw3f)
	list(APPEND dumphfdl_extra_sources hfdl_fft_fftw.c)
	list(APPEND dumphfdl_extra_libs ${FFTW_LIBRARIES})
	list(APPEND dumphfdl_include_dirs ${FFTW_INCLUDE_DIRS})
	list(APPEND link_dirs ${FFTW_LIBRARY_DIRS})
	find_library(FFTW3F_THREADS_FOUND fftw3f_threads)
# Kiwi
#	if(FFTW3F_THREADS_FOUND)
	if(FALSE)
		list(APPEND dumphfdl_extra_libs fftw3f_threads)
		set(WITH_FFTW3F_THREADS TRUE)
	else()
		message(WARNING "libfftw3f_threads library not found, single-threaded implementation will be used")
		set(WITH_FFTW3F_THREADS FALSE)
	endif()
else()
	message(FATAL_ERROR "Unknown FFT library: ${FFT_LIBRARY}")
endif()

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
list(APPEND dumphfdl_extra_libs ${GLIB2_LIBRARIES})
list(APPEND dumphfdl_include_dirs ${GLIB2_INCLUDE_DIRS})
list(APPEND link_dirs ${GLIB2_LIBRARY_DIRS})

pkg_check_modules(ACARS REQUIRED libacars-2>=2.1.0)
list(APPEND dumphfdl_extra_libs ${ACARS_LIBRARIES})
list(APPEND dumphfdl_include_dirs ${ACARS_INCLUDE_DIRS})
list(APPEND link_dirs ${ACARS_LIBRARY_DIRS})

pkg_check_modules(CONFIG REQUIRED libconfig++)
list(APPEND dumphfdl_extra_libs ${CONFIG_LIBRARIES})
list(APPEND dumphfdl_include_dirs ${CONFIG_INCLUDE_DIRS})
list(APPEND link_dirs ${CONFIG_LIBRARY_DIRS})

option(SOAPYSDR "Enable SoapySDR support" ON)
set(WITH_SOAPYSDR FALSE)

option(ETSY_STATSD "Enable Etsy StatsD support" ON)
set(WITH_STATSD FALSE)

option(SQLITE "Enable SQLite support for reading aircraft info from
Basestation DB" ON)
set(WITH_SQLITE FALSE)

option(ZMQ "Enable support for ZeroMQ outputs" ON)
set(WITH_ZMQ FALSE)

option(PROFILING "Enable profiling with gperftools")
set(WITH_PROFILING FALSE)

if(SOAPYSDR)
	message(STATUS "Checking for SoapySDR")
	find_package(SoapySDR NO_MODULE)
	if(SoapySDR_FOUND)
		list(APPEND dumphfdl_extra_sources input-soapysdr.c)
		message(STATUS "  SoapySDR found, ${SoapySDR_INCLUDE_DIRS}, ${SoapySDR_LIBRARIES}")
		list(APPEND dumphfdl_extra_libs ${SoapySDR_LIBRARIES})
		list(APPEND dumphfdl_include_dirs ${SoapySDR_INCLUDE_DIRS})
		set(WITH_SOAPYSDR TRUE)
	else()
		message(STATUS "  SoapySDR not found")
	endif()
endif()

if(ETSY_STATSD)
	find_library(STATSD_FOUND statsdclient)
	if(STATSD_FOUND)
		list(APPEND dumphfdl_extra_sources statsd.c)
		list(APPEND dumphfdl_extra_libs statsdclient)
		set(WITH_STATSD TRUE)
	endif()
endif()

if(SQLITE)
	find_package(SQLite3)
	if (SQLite3_FOUND)
		list(APPEND dumphfdl_extra_libs ${SQLite3_LIBRARIES})
		list(APPEND dumphfdl_include_dirs ${SQLite3_INCLUDE_DIRS})
		set(WITH_SQLITE TRUE)
	endif()
endif()

set(LIBZMQ_VER_MAJOR_MIN 3)
set(LIBZMQ_VER_MINOR_MIN 2)
set(LIBZMQ_VER_PATCH_MIN 0)
set(LIBZMQ_VER_MIN
	"${LIBZMQ_VER_MAJOR_MIN}.${LIBZMQ_VER_MINOR_MIN}.${LIBZMQ_VER_PATCH_MIN}")
if(ZMQ)
	pkg_check_modules(ZMQ libzmq>=${LIBZMQ_VER_MIN})
	if(ZMQ_FOUND)
		list(APPEND dumphfdl_extra_sources output-zmq.c)
		list(APPEND dumphfdl_extra_libs ${ZMQ_LIBRARIES})
		list(APPEND dumphfdl_include_dirs ${ZMQ_INCLUDE_DIRS})
		list(APPEND link_dirs ${ZMQ_LIBRARY_DIRS})
		set(WITH_ZMQ TRUE)
	endif()
endif()

if(PROFILING)
	pkg_check_modules(PROFILING libprofiler)
	if(PROFILING_FOUND)
		list(APPEND dumphfdl_extra_libs ${PROFILING_LIBRARIES})
		list(APPEND dumphfdl_include_dirs ${PROFILING_INCLUDE_DIRS})
		list(APPEND link_dirs ${PROFILING_LIBRARY_DIRS})
		set(WITH_PROFILING TRUE)
	endif()
endif()

message(STATUS "dumphfdl configuration summary:")
message(STATUS "- SDR drivers:")
message(STATUS "  - soapysdr:\t\trequested: ${SOAPYSDR}, enabled: ${WITH_SOAPYSDR}")
message(STATUS "- Other options:")
message(STATUS "  - Etsy StatsD:\t\trequested: ${ETSY_STATSD}, enabled: ${WITH_STATSD}")
message(STATUS "  - SQLite:\t\t\trequested: ${SQLITE}, enabled: ${WITH_SQLITE}")
message(STATUS "  - ZeroMQ:\t\t\trequested: ${ZMQ}, enabled: ${WITH_ZMQ}")
message(STATUS "  - Profiling:\t\trequested: ${PROFILING}, enabled: ${WITH_PROFILING}")
message(STATUS "  - Multithreaded FFT:\t${WITH_FFTW3F_THREADS}")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/hfdl_config.h.in"
	"${CMAKE_CURRENT_SOURCE_DIR}/hfdl_config.h"
	@ONLY
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/version.c
		${CMAKE_CURRENT_BINARY_DIR}/_version.c
	COMMAND ${CMAKE_COMMAND} -DDUMPHFDL_VERSION=${DUMPHFDL_VERSION} -P ${CMAKE_CURRENT_SOURCE_DIR}/version.cmake
)

add_subdirectory (libfec)

add_library (dumphfdl_base OBJECT
	ac_cache.c
	ac_data.c
	acars.c
	block.c
	cache.c
	crc.c
	fastddc.c
	fmtr-basestation.c
	fmtr-text.c
	globals.c
	hfdl_chan.c
	hfdl_fft.c
	hfdl_main.c
	hfnpdu.c
	input-buf.c
	input-common.c
	input-file.c
	input-helpers.c
	kvargs.c
	libcsdr.c
	libcsdr_gpl.c
	lpdu.c
	metadata.c
	mpdu.c
	options.c
	output-common.c
	output-file.c
	output-tcp.c
	output-udp.c
	pdu.c
	position.c
	spdu.c
	systable.c
	util.c
	${CMAKE_CURRENT_BINARY_DIR}/version.c
	${dumphfdl_extra_sources}
)

# Kiwi
target_include_directories (dumphfdl_base PUBLIC
	${dumphfdl_include_dirs}
	../../../../
	../../../../support
	../../../../platform/common
	../../../../pkgs/mongoose
)

# can't do this per target with cmake <3.13
link_directories(${link_dirs})

set(dumphfdl_obj_files
	$<TARGET_OBJECTS:dumphfdl_base>
	$<TARGET_OBJECTS:fec>
)

add_executable (dumphfdl ${dumphfdl_obj_files})

target_link_libraries (dumphfdl
	m
	pthread
	${dumphfdl_extra_libs}
)

install(TARGETS dumphfdl
	RUNTIME DESTINATION bin
)
