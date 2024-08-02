# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.5)

# Define the source files
set(SOURCES 
    "main.c" 
    "led_handler.c"
    "certs/AmazonRootCA1_pem.c"
    "certs/home_hallway_bathroom_lights_certificate_pem.c"
    "certs/home_hallway_bathroom_lights_private_pem_key.c"
)

# Specify the directory containing the header files
set(INCLUDES ".")

# Add the source files to the build
idf_component_register(
    SRCS 
        ${SOURCES}
    INCLUDE_DIRS 
        ${INCLUDES}
    REQUIRES 
        mbedtls 
        app_update 
        esp_https_ota 
        mqtt 
    PRIV_REQUIRES 
        mbedtls 
        nvs_flash 
        esp_wifi 
        driver 
        json
        gecl-ota-manager
        gecl-wifi-manager
        gecl-time-sync-manager
        gecl-logger-manager
        gecl-heartbeat-manager
        gecl-mqtt-manager
        gecl-misc-util-manager
        gecl-versioning-manager
        gecl-telemetry-manager
)