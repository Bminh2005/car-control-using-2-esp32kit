# Khai báo file source, thư mục chứa file header và các thư viện phụ thuộc
idf_component_register(SRCS "l298n_driver.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver)