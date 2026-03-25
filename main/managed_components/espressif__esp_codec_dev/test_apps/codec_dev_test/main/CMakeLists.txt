
set(priv_requires
        unity
        driver
        esp_codec_dev
)

idf_component_register(SRC_DIRS .
                       PRIV_INCLUDE_DIRS .
                       PRIV_REQUIRES unity esp_codec_dev
                       PRIV_REQUIRES ${priv_requires}
                       WHOLE_ARCHIVE TRUE
                       SRCS "test_board.c" "test_app_main.c" ...
                       INCLUDE_DIRS "."
                       REQUIRES esp_common  # 必须包含这个                      
                       )
