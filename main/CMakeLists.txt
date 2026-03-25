

idf_component_register(SRCS
                            "main.c"
                            "bsp_wifi.c"
                            "bsp_wifi.c"
                            "bsp/bsp_http_server.c"
                            "bsp/bsp_i2c.c"
                            "bsp/bsp.c"
                            "bsp/bsp_i2s.c"
                            "bsp/bsp_es8311.c"
                            "bsp/bsp_pca9557.c"
                            "bsp/bsp_tf.c"
                            "bsp/bsp_music_play.c"
                            "bsp/bsp_led.c"
                            "bsp/bsp_breathing_led.c"
                            "bsp/bsp_magent.c"
                            "bsp/bsp_drum.c"
                            "bsp/bsp_pa.c"
                            "bsp/bsp_key.c"
                            "bsp/bsp_keyhandle.c"
                            "bsp/bsp_adc.c"
                    INCLUDE_DIRS "./"
                                  "bsp"
                    )

idf_component_get_property(esp_common_dir esp_common COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${esp_common_dir}/include")
