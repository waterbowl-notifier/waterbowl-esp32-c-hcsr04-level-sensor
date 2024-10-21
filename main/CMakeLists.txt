
# Define the source files
set(SOURCES 
    "main.c" 
    "hcsr04.c"
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
    add_compile_definitions(MAC_ADDR="${CONFIG_LIVING_ROOM_MAC}")
    add_compile_definitions(GPIO_HCSR04_TRIGGER_PIN=${CONFIG_LIVING_ROOM_GPIO_HCSR04_TRIGGER_PIN})
    add_compile_definitions(GPIO_HCSR04_ECHO_PIN=${CONFIG_LIVING_ROOM_GPIO_HCSR04_ECHO_PIN})
    add_compile_definitions(WATERBOWL_ESP32_SLEEP_MINUTES=${CONFIG_LIVING_ROOM_WATERBOWL_ESP32_SLEEP_MINUTES})
    add_compile_definitions(WIFI_SSID="${CONFIG_LIVING_ROOM_WIFI_SSID}")
    add_compile_definitions(WIFI_PASSWORD="${CONFIG_LIVING_ROOM_WIFI_PASSWORD}")
    set(CERT_HEADER "certs/WaterBowl_LivingRoom-certificate.h")
    set(KEY_HEADER "certs/WaterBowl_LivingRoom-private.pem.h")
elseif(CONFIG_DINING_ROOM)
    add_compile_definitions(DINING_ROOM)
    add_compile_definitions(MAC_ADDR="${CONFIG_DINING_ROOM_MAC}")
    add_compile_definitions(GPIO_HCSR04_TRIGGER_PIN=${CONFIG_DINING_ROOM_GPIO_HCSR04_TRIGGER_PIN})
    add_compile_definitions(GPIO_HCSR04_ECHO_PIN=${CONFIG_DINING_ROOM_GPIO_HCSR04_ECHO_PIN})
    add_compile_definitions(WATERBOWL_ESP32_SLEEP_MINUTES=${CONFIG_DINING_ROOM_WATERBOWL_ESP32_SLEEP_MINUTES})
    add_compile_definitions(WIFI_SSID="${CONFIG_DINING_ROOM_WIFI_SSID}")
    add_compile_definitions(WIFI_PASSWORD="${CONFIG_DINING_ROOM_WIFI_PASSWORD}")
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
