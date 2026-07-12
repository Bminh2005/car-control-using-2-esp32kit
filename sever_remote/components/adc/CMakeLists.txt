# Khai báo file source, thư mục chứa file header và các thư viện phụ thuộc
idf_component_register(SRCS "adc_module.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_adc)