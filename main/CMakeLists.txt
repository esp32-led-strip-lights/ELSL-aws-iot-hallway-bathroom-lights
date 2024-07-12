# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

# Define the source files
set(SOURCES 
    "main.c" 
    "wifi.c" 
    "mqtt.c" 
    "ota.c"
    "logging.c"
    "motion_sensor.c"
    "led_handler.c"
    "certs/amazon_root_ca1.c"
    "certs/home_hallway_bathroom_lights_certificate_pem.c"
    "certs/home_hallway_bathroom_lights_private_pem_key.c"
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
