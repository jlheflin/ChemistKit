install(
    TARGETS ChemisKit_exe
    RUNTIME COMPONENT ChemisKit_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
