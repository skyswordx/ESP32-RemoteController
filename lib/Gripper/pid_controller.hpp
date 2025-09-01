/**
 * @file pid_controller.hpp
 * @brief 增强型PID控制器C++类实现 - 基于RoboWalker Class_PID设计
 * @version 2.0
 * @date 2025-08-25
 * @author GitHub Copilot
 * 
 * @copyright Copyright (c) 2025 SENTRY Team
 * 
 * @note 简洁C++实现，直接将C结构体转换为C++类，C函数转换为成员函数
 *       保持与原C版本完全相同的算法和接口
 */

#ifndef PID_CONTROLLER_HPP
#define PID_CONTROLLER_HPP

/* Includes ------------------------------------------------------------------*/
#include "math_utils.h"

/* Private macros ------------------------------------------------------------*/
#define PID_EPSILON                 (1e-6f)     // 浮点数比较精度

/* Exported types ------------------------------------------------------------*/

/**
 * @brief PID控制器状态枚举
 */
enum class PID_state_c : uint8_t {
    PID_STATE_STOP = 0,         ///< 停止状态
    PID_STATE_NORMAL,           ///< 正常控制状态
    PID_STATE_SATURATED,        ///< 饱和状态
    PID_STATE_DEAD_ZONE         ///< 死区状态
};

/**
 * @brief 增强型PID控制器C++类
 * 
 * @note 直接从C结构体转换而来，保持相同的内存布局和算法
 *       实现变速积分、积分分离、微分先行等RoboWalker核心特性
 */
class PID_controller_c {
protected:
    /* PID参数 */
    float kp;                   ///< 比例增益
    float ki;                   ///< 积分增益
    float kd;                   ///< 微分增益
    float kf;                   ///< 前馈增益
    
    /* 控制配置 */
    float dt;                   ///< 控制周期（秒）
    float dead_zone;            ///< 死区阈值
    float output_limit;         ///< 输出限幅值（0为不限制）
    float integral_limit;       ///< 积分限幅值（0为不限制）
    
    /* 变速积分参数（RoboWalker核心算法） */
    float i_variable_speed_a;   ///< 变速积分定速区阈值
    float i_variable_speed_b;   ///< 变速积分变速区阈值
    
    /* 积分分离参数 */
    float i_separate_threshold; ///< 积分分离阈值（0为不启用）
    
    /* 增强功能控制 */
    bool enable_d_first;        ///< 微分先行使能（true-对反馈值微分，false-对误差微分）
    bool enable_integral_limit; ///< 积分限幅使能
    bool enable_output_limit;   ///< 输出限幅使能
    
    /* 状态变量 */
    float target;               ///< 目标值
    float feedback;             ///< 反馈值
    float output;               ///< 输出值
    float error;                ///< 当前误差
    float integral_error;       ///< 积分误差
    
    /* 历史记录（用于微分计算） */
    float pre_feedback;         ///< 上一次反馈值
    float pre_target;           ///< 上一次目标值
    float pre_error;            ///< 上一次误差
    float pre_output;           ///< 上一次输出值
    
    /* 控制状态 */
    PID_state_c state;             ///< 控制器状态
    
    /* 调试和统计信息 */
    float p_out;                ///< P项输出（调试用）
    float i_out;                ///< I项输出（调试用）
    float d_out;                ///< D项输出（调试用）
    float f_out;                ///< 前馈输出（调试用）
    float max_error;            ///< 历史最大误差
    uint32_t update_count;      ///< 更新次数
    
    /* 私有辅助函数 */
    void Update_state();
    
public:
    /* 构造函数和析构函数 */
    
    /**
     * @brief 默认构造函数
     */
    PID_controller_c();
    
    /**
     * @brief 基本构造函数
     * @param kp 比例增益
     * @param ki 积分增益
     * @param kd 微分增益
     */
    PID_controller_c(float kp, float ki, float kd);
    
    /**
     * @brief 析构函数
     */
    ~PID_controller_c() = default;
    
    /* 初始化函数 */
    
    /**
     * @brief PID控制器基础初始化
     * @param kp 比例增益
     * @param ki 积分增益
     * @param kd 微分增益
     */
    void Init(float kp, float ki, float kd);
    
    /**
     * @brief PID控制器完整初始化（RoboWalker完整配置）
     * @param kp 比例增益
     * @param ki 积分增益  
     * @param kd 微分增益
     * @param kf 前馈增益
     * @param integral_limit 积分限幅值
     * @param output_limit 输出限幅值
     * @param dt 控制周期
     * @param dead_zone 死区阈值
     * @param i_variable_speed_a 变速积分定速区阈值
     * @param i_variable_speed_b 变速积分变速区阈值
     * @param i_separate_threshold 积分分离阈值
     * @param d_first 微分先行使能
     */
    void Init_full(float kp, float ki, float kd, float kf,
                   float integral_limit, float output_limit, float dt,
                   float dead_zone, float i_variable_speed_a, float i_variable_speed_b,
                   float i_separate_threshold, bool d_first);
    
    /* 主控制函数 */
    
    /**
     * @brief PID控制器主更新函数（严格按照RoboWalker算法实现）
     * @param target 目标值
     * @param feedback 反馈值
     * @return 控制器输出
     */
    float Update_period(float target, float feedback);
    
    /* 参数设置函数 */
    
    /**
     * @brief 设置PID参数（批量设置）
     * @param kp 比例增益
     * @param ki 积分增益
     * @param kd 微分增益
     */
    void Set_params(float kp, float ki, float kd);
    
    /**
     * @brief 设置比例增益
     * @param kp 比例增益
     */
    inline void Set_p(float kp);
    
    /**
     * @brief 设置积分增益
     * @param ki 积分增益
     */
    inline void Set_i(float ki);
    
    /**
     * @brief 设置微分增益
     * @param kd 微分增益
     */
    inline void Set_d(float kd);
    
    /**
     * @brief 设置前馈增益
     * @param kf 前馈增益
     */
    inline void Set_feedforward(float kf);
    
    /**
     * @brief 设置变速积分参数
     * @param threshold_a 定速区阈值
     * @param threshold_b 变速区阈值
     */
    void Set_variable_integral(float threshold_a, float threshold_b);
    
    /**
     * @brief 设置积分分离阈值
     * @param threshold 积分分离阈值
     */
    inline void Set_integral_separation(float threshold);
    
    /**
     * @brief 设置输出限幅
     * @param limit 输出限幅值
     */
    inline void Set_output_limit(float limit);
    
    /**
     * @brief 设置积分限幅
     * @param limit 积分限幅值
     */
    inline void Set_integral_limit(float limit);
    
    /**
     * @brief 设置死区阈值
     * @param dead_zone 死区阈值
     */
    inline void Set_dead_zone(float dead_zone);
    
    /**
     * @brief 启用/禁用微分先行
     * @param enable 微分先行使能
     */
    inline void Set_derivative_first(bool enable);
    
    /* 状态管理函数 */
    
    /**
     * @brief 重置PID控制器
     */
    void Reset();
    
    /**
     * @brief 清零积分项
     */
    void Clear_integral();

};

/* Inline function implementations ------------------------------------------*/

inline void PID_controller_c::Set_p(float kp) {
    this->kp = kp;
}

inline void PID_controller_c::Set_i(float ki) {
    this->ki = ki;
}

inline void PID_controller_c::Set_d(float kd) {
    this->kd = kd;
}

inline void PID_controller_c::Set_feedforward(float kf) {
    this->kf = kf;
}

inline void PID_controller_c::Set_integral_separation(float threshold) {
    this->i_separate_threshold = Math_abs(threshold);
}

inline void PID_controller_c::Set_output_limit(float limit) {
    this->output_limit = Math_abs(limit);
    this->enable_output_limit = (limit > PID_EPSILON);
}

inline void PID_controller_c::Set_integral_limit(float limit) {
    this->integral_limit = Math_abs(limit);
    this->enable_integral_limit = (limit > PID_EPSILON);
}

inline void PID_controller_c::Set_dead_zone(float dead_zone) {
    this->dead_zone = Math_abs(dead_zone);
}

inline void PID_controller_c::Set_derivative_first(bool enable) {
    this->enable_d_first = enable;
}

#endif /* PID_CONTROLLER_HPP */

/************************ COPYRIGHT(C) SENTRY TEAM **************************/
