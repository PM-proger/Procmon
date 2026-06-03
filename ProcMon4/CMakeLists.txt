project ("ProcMon4")

message("========= ProcMon4 source dir> " + ${CMAKE_SOURCE_DIR})				

add_executable(ProcMon4 WIN32 ProcMon4.cpp 
				Job_SendDLL.cpp Job_RecvDLL.cpp
				ProcMon4.rc
)

target_link_libraries(ProcMon4 PUBLIC UtilsProc_LIB)

target_include_directories(ProcMon4 PRIVATE 
		${CMAKE_SOURCE_DIR}/UtilsProc_LIB
		)