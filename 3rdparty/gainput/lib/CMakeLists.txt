project(gainput)

set(CMAKE_MACOSX_RPATH 1)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -Wall -Wextra -pedantic -Wshadow -Wno-variadic-macros")
endif()

file(GLOB_RECURSE sources source/*.cpp source/*.h include/*.h)

if(APPLE)
	file(GLOB_RECURSE mmsources source/*.mm)
endif()

add_library(gainput STATIC ${sources} ${mmsources})
# set_target_properties(gainput PROPERTIES DEBUG_POSTFIX -d FOLDER gainput)
target_include_directories(gainput PUBLIC ./include)

if (MSVC)
    target_compile_options(gainput PRIVATE /wd4267 /wd4244)
endif()

if(WIN32)
	target_link_libraries(gainput PUBLIC ${XINPUT} ws2_32)
elseif(ANDROID)
	target_link_libraries(gainput PUBLIC native_app_glue log android)
elseif(APPLE)
	find_library(FOUNDATION Foundation)
	find_library(IOKIT IOKit)
  find_library(GAME_CONTROLLER GameController)
	target_link_libraries(gainput PUBLIC ${FOUNDATION} ${IOKIT} ${GAME_CONTROLLER})
  if(IOS)
      find_library(UIKIT UIKit)
      find_library(COREMOTION CoreMotion)
      find_library(QUARTZCORE QuartzCore)
      target_link_libraries(gainput PUBLIC ${UIKIT} ${COREMOTION})
  else()
      find_library(APPKIT AppKit)
      target_link_libraries(gainput PUBLIC ${APPKIT})
  endif()
endif()

if (NOT MSVC)
  target_compile_options(gainput PUBLIC -Wno-implicit-fallthrough -Wno-unused-parameter)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(gainput PUBLIC -Wno-stringop-truncation)
endif()