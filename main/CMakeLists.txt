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
        nvs_flash 
        mbedtls 
        app_update 
        esp_https_ota 
        mqtt 
        driver 
        esp_wifi 
        json
    PRIV_REQUIRES 
        gecl-ota-manager
        gecl-wifi-manager
        gecl-time-sync-manager
        gecl-logger-manager
        gecl-heartbeat-manager
        gecl-mqtt-manager
        gecl-misc-util-manager
        gecl-versioning-manager
        gecl-telemetry-manager
        gecl-motion-sensor-manager
)