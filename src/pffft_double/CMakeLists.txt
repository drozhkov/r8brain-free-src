#
cmake_minimum_required (VERSION 3.8)


#
project ("pffft-double")


#
add_library (${PROJECT_NAME} 
	pffft_double.c
)
