# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

# Define the source files
set(SOURCES 
    "main.c" 
    "wifi.c" 
    "mqtt.c" 
    "led.c" 
    "state_handler.c"
    "sensors.c"
    "ota.c"
    "logging.c"
    "certs/amazon_root_ca1.c"
    "certs/chicken_coop_controller_cert.c"
    "certs/chicken_coop_controller_private_key.c"
)

# Specify the directory containing the header files
set(INCLUDES ".")

# Add the source files to the build
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS ${INCLUDES}
    REQUIRES mbedtls app_update esp_https_ota
    PRIV_REQUIRES mbedtls mqtt nvs_flash esp_wifi driver json
)
