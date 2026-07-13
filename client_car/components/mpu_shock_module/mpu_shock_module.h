#ifndef MPU_SHOCK_MODULE_H
#define MPU_SHOCK_MODULE_H

// Đoạn mã này giúp trình biên dịch C (gcc) và C++ (g++) hiểu nhau
#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Khởi tạo giao tiếp I2C và Task chạy ngầm cho MPU6050
     * Gọi hàm này 1 lần duy nhất trong app_main()
     */
    void mpu_task_init(void);

#ifdef __cplusplus
}
#endif

#endif // MPU_SHOCK_MODULE_H