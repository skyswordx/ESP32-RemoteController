/**
 * @file math_utils.h
 * @brief 数学工具库头文件 - 替换原有的Math_Utils依赖
 * @version 1.0
 * @date 2025-08-27
 * @author GitHub Copilot
 * 
 * @copyright Copyright (c) 2025 SENTRY Team
 * 
 * @note 为PID控制器和斜坡规划器提供必要的数学工具函数
 */

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Math utility functions ---------------------------------------------------*/

/**
 * @brief 计算浮点数的绝对值
 * @param value 输入值
 * @return float 绝对值
 */
static inline float Math_abs(float value) {
    return fabsf(value);
}

/**
 * @brief 限制数值在指定范围内
 * @param value 输入值
 * @param min_value 最小值
 * @param max_value 最大值
 * @return float 限制后的值
 */
static inline float Math_constrain_value(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    } else if (value > max_value) {
        return max_value;
    } else {
        return value;
    }
}

/**
 * @brief 线性插值函数
 * @param x1 起始值
 * @param x2 结束值
 * @param t 插值参数 (0.0 - 1.0)
 * @return float 插值结果
 */
static inline float Math_lerp(float x1, float x2, float t) {
    return x1 + t * (x2 - x1);
}

/**
 * @brief 将弧度转换为角度
 * @param rad 弧度值
 * @return float 角度值
 */
static inline float Math_rad_to_deg(float rad) {
    return rad * 180.0f / M_PI;
}

/**
 * @brief 将角度转换为弧度
 * @param deg 角度值
 * @return float 弧度值
 */
static inline float Math_deg_to_rad(float deg) {
    return deg * M_PI / 180.0f;
}

/**
 * @brief 计算两个浮点数是否近似相等
 * @param a 第一个数
 * @param b 第二个数
 * @param epsilon 误差范围
 * @return bool 是否近似相等
 */
static inline bool Math_is_equal(float a, float b, float epsilon) {
    return Math_abs(a - b) < epsilon;
}

#ifdef __cplusplus
}
#endif

#endif /* MATH_UTILS_H */

/************************ COPYRIGHT(C) SENTRY TEAM **************************/
