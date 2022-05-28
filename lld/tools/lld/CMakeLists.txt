set(LLVM_LINK_COMPONENTS
  Support
  )

add_lld_tool(lld
  lld.cpp

  SUPPORT_PLUGINS
  )
export_executable_symbols_for_plugins(lld)

if (HAIKU)
target_link_libraries(lld
  PRIVATE
  lldCommon
  lldCOFF
  lldDriver
  lldELF
  lldMinGW
  lldWasm
  )
else()
target_link_libraries(lld
  PRIVATE
  lldCommon
  lldCOFF
  lldDriver
  lldELF
  lldMachO2
  lldMinGW
  lldWasm
  )
endif()

install(TARGETS lld
  RUNTIME DESTINATION bin)

if(NOT LLD_SYMLINKS_TO_CREATE)
  set(LLD_SYMLINKS_TO_CREATE
      lld-link ld.lld ld64.lld ld64.lld.darwinnew wasm-ld)
endif()

foreach(link ${LLD_SYMLINKS_TO_CREATE})
  add_lld_symlink(${link} lld)
endforeach()
