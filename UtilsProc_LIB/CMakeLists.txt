project ("UtilsProc_LIB")

add_library(UtilsProc_LIB STATIC
				UtilsMisc_LIB.cpp UtilsMisc_LIB.h
				UtilsNet_LIB.cpp UtilsNet_LIB.h
				UtilsProc_LIB.cpp UtilsProc_LIB.h
				UtilsWin_LIB.cpp UtilsWin_LIB.h
				)

