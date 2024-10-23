
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
    file(GLOB_RECURSE CERT_SOURCES "certs/c/living/*.c")
elseif(CONFIG_LOCATION_DINING_ROOM)
    file(GLOB_RECURSE CERT_SOURCES "certs/c/dining/*.c")
endif()

set(AWS_SOURCE "certs/c/AmazonRootCA1_pem.c")

idf_component_register(
    SRCS 
        ${SOURCES} 
        ${CERT_SOURCES}
        ${COMPONENT_SOURCES}
        ${AWS_SOURCE}
    INCLUDE_DIRS 
        ${INCLUDES} 
        ${COMPONENT_INCLUDES}
)

message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
