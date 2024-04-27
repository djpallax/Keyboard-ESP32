idf_component_register(SRCS "main.c"
                            "esp_hidd_prf_api.c"
                            "hid_device_le_prf.c"
                            "hid_dev.c"
                    INCLUDE_DIRS "." "${CMAKE_SOURCE_DIR}/include")

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
