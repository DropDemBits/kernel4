list(APPEND ACPICA_SOURCES
    source/components/resources/rsaddr.c
    source/components/resources/rscalc.c
    source/components/resources/rscreate.c
    source/components/resources/rsdumpinfo.c
    source/components/resources/rsinfo.c
    source/components/resources/rsio.c
    source/components/resources/rsirq.c
    source/components/resources/rslist.c
    source/components/resources/rsmemory.c
    source/components/resources/rsmisc.c
    source/components/resources/rsserial.c
    source/components/resources/rsutils.c
    source/components/resources/rsxface.c)

if(ENABLE_ACPI_AML_DEBUGGER)
list(APPEND ACPICA_SOURCES
    source/components/resources/rsdump.c)
endif()