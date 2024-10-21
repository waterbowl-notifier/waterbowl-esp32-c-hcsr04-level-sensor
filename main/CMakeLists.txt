
# Define the source files
set(SOURCES 
    "main.c" 
)

set(INCLUDES 
    "."
    "certs"
)

# Initialize variables to prevent errors if not set
set(CERT_HEADER "")
set(KEY_HEADER "")

# Determine which configuration is selected
if(CONFIG_LIVING_ROOM)
    add_compile_definitions(LIVING_ROOM)
    set(CERT_HEADER "certs/WaterBowl_LivingRoom-certificate.h")
    set(KEY_HEADER "certs/WaterBowl_LivingRoom-private.pem.h")
elseif(CONFIG_DINING_ROOM)
    add_compile_definitions(DINING_ROOM)
    set(CERT_HEADER "certs/WaterBowl_DiningRoom-certificate.h")
    set(KEY_HEADER "certs/WaterBowl_DiningRoom-private.pem.h")
endif()

idf_component_register(
    SRCS 
        ${SOURCES} 
    INCLUDE_DIRS 
        ${INCLUDES} 
    EMBED_TXTFILES 
        ${CERT_HEADER} 
        ${KEY_HEADER}
)

message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
