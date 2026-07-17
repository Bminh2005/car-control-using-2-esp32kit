idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." "../protocol"
                    REQUIRES bt nvs_flash driver)