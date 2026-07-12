# Khai báo file source, thư mục chứa file header và các thư viện phụ thuộc
idf_component_register(SRCS "protocol.c"
                       INCLUDE_DIRS "." "../components/adc"
                       REQUIRES esp_adc)