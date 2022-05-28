set(LLVM_LINK_COMPONENTS
  OrcShared
  OrcTargetProcess
  Support
  )

add_llvm_utility(llvm-jitlink-executor
  llvm-jitlink-executor.cpp

  DEPENDS
  intrinsics_gen
)

if( HAIKU )
  target_link_libraries(llvm-jitlink-executor PRIVATE network)
endif()

export_executable_symbols(llvm-jitlink-executor)
