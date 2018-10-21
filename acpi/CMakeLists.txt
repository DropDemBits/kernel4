
# Component Sources
include(source/components/dispatcher/sources.cmake)
include(source/components/events/sources.cmake)
include(source/components/executer/sources.cmake)
include(source/components/hardware/sources.cmake)
include(source/components/namespace/sources.cmake)
include(source/components/parser/sources.cmake)
include(source/components/resources/sources.cmake)
include(source/components/tables/sources.cmake)
include(source/components/utilities/sources.cmake)

add_library(acpi ${ACPICA_SOURCES})
