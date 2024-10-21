
# Define the source files
set(SOURCES 
    "main.c" 
)

set(INCLUDES 
    "."
 )

# Get all the component include directories
file(GLOB_RECURSE COMPONENT_SOURCES "components/*/*.c")

file(GLOB_RECURSE COMPONENT_INCLUDES "components/*/include")

# Determine which configuration is selected
if(CONFIG_LOCATION_LIVING_ROOM)
    set(CERT_INCLUDES  "certs/include/living")
elseif(CONFIG_LOCATION_DINING_ROOM)
    set(CERT_INCLUDES  "certs/include/dining")
endif()

idf_component_register(
    SRCS 
        ${SOURCES} 
        ${COMPONENT_SOURCES}
    INCLUDE_DIRS 
        ${INCLUDES} 
        ${COMPONENT_INCLUDES}
        ${CERT_INCLUDES}
)

message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
