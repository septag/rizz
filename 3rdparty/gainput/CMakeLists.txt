cmake_minimum_required(VERSION 2.8)
set(GAINPUT_MAJOR_VERSION 1)
set(GAINPUT_MINOR_VERSION 0)
set(GAINPUT_PATCH_VERSION 0)
set(GAINPUT_VERSION ${GAINPUT_MAJOR_VERSION}.${GAINPUT_MINOR_VERSION}.${GAINPUT_PATCH_VERSION})

if(!WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -Wextra")
else()
	set(XINPUT "Xinput9_1_0")
	if ( ${CMAKE_SYSTEM_VERSION} LESS 6.1 )
		set(XINPUT, "xinput")
	endif()
endif()

rizz__remove_compile_options(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}" "-fobjc-arc")
rizz__remove_compile_options(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" "-fobjc-arc")

if(ANDROID)
	include(AndroidNdkModules)
	android_ndk_import_module_native_app_glue()
endif()

add_subdirectory(lib)

