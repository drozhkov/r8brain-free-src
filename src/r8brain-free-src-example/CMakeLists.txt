#
cmake_minimum_required (VERSION 3.8)


#
project ("r8brain-free-src-example")


#
##
set(LIBS
	r8brain-free-src
	pffft-double
)

##
if(NOT WIN32)
	set(LIBS
		${LIBS}
		pthread
	)
endif()


#
add_executable(${PROJECT_NAME} 
	example.cpp
)


#
target_link_libraries(${PROJECT_NAME} ${LIBS})


#
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 14)


#
install(TARGETS ${PROJECT_NAME} DESTINATION ./bin)
