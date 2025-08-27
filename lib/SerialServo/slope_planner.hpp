/**
 * @file slope_planner.hpp
 * @brief 斜坡规划器C++类头文件 - 完全按照RoboWalker Class_Slope设计
 * @version 3.0
 * @date 2025-08-26
 * @author GitHub Copilot
 * 
 * @copyright Copyright (c) 2025 SENTRY Team
 * 
 * @note 完全按照RoboWalker设计文档实现，重点理解真实值优先的设计思想：
 *       参考：RM-Sentry/reference/RoboWalker_Core_Algorithms/1_Algorithm_Base/斜坡规划器设计文档.md
 *       
 * 核心特性：
 * 1. 真实值优先：以真实值为新起点，继续朝目标进行斜坡规划（不是简单跳转）
 * 2. 平滑过渡：限制变化率，避免突变和冲击
 * 3. 分段处理：正值、负值、零值的不同加速减速策略  
 * 4. 变量角色：out对外输出，now_planning维护内部状态
 */

#ifndef SLOPE_PLANNER_HPP
#define SLOPE_PLANNER_HPP

/* Includes ------------------------------------------------------------------*/
#include "math_utils.h"

/* Exported types ------------------------------------------------------------*/

/**
 * @brief 斜坡规划器C++类
 * 
 * @note 直接从C结构体转换而来，保持相同的内存布局和算法
 *       实现RoboWalker斜坡规划核心特性
 */
class Slope_planner_c {
protected:
    /* 规划参数 */
    float increase_value;      ///< 上升斜率（绝对值增量，一次计算周期改变值）
    float decrease_value;      ///< 下降斜率（绝对值减量，一次计算周期改变值）
    float target;             ///< 目标值
    
    /* 状态变量 */
    float now_planning;       ///< 当前规划值
    float now_real;           ///< 当前真实值
    float out;                ///< 输出值

    /* 控制参数 */
    bool real_first;          ///< 真实值优先使能（true-真实值优先，false-目标值优先）
    
public:
    /* 构造函数和析构函数 */
    
    /**
     * @brief 默认构造函数
     */
    Slope_planner_c();
    
    /**
     * @brief 基本构造函数
     * @param inc 上升斜率
     * @param dec 下降斜率
     * @param real_first 真实值优先使能（true-真实值优先，false-目标值优先）
     */
    Slope_planner_c(float inc, float dec, bool real_first);
    
    /**
     * @brief 析构函数
     */
    ~Slope_planner_c() = default;
    
    /* 初始化函数 */
    
    /**
     * @brief 斜坡规划器初始化
     * @param inc 上升斜率（绝对值增量，一次计算周期改变值）
     * @param dec 下降斜率（绝对值减量，一次计算周期改变值）
     * @param real_first 真实值优先使能（true-真实值优先，false-目标值优先）
     */
    void Init(float inc, float dec, bool real_first = true);
    
    /* 主控制函数 */
    
    /**
     * @brief 斜坡规划器周期更新函数（完全按照RoboWalker设计文档实现）
     * 
     * @note 对应RoboWalker的TIM_Calculate_PeriodElapsedCallback函数
     *       需要在定时中断或RTOS任务中周期性调用
     *       
     *       关键设计思想（参考设计文档6.2.1节）：
     *       - 真实值优先：以真实值为新起点，继续朝目标进行斜坡规划
     *       - 示例：Target=2.0, Planning=1.5, Real=1.7 → Out=1.7+0.1=1.8
     *       - 既响应了真实状态，又保持了规划的连续性
     */
    void Update_period();
    
    /* 参数设置函数 */
    
    /**
     * @brief 设置斜坡规划器目标值
     * @param target 新的目标值
     */
    inline void Set_target(float target);
    
    /**
     * @brief 设置上升斜率
     * @param inc 上升斜率
     */
    inline void Set_increase_value(float inc);
    
    /**
     * @brief 设置下降斜率
     * @param dec 下降斜率
     */
    inline void Set_decrease_value(float dec);
    
    /**
     * @brief 设置当前真实值
     * @param real_value 当前真实值
     * 
     * @note 用于真实值优先模式，在Update_period()调用前设置
     */
    inline void Set_now_real(float real_value);
    
    /**
     * @brief 设置真实值优先模式
     * @param real_first 真实值优先使能（true-真实值优先，false-目标值优先）
     */
    inline void Set_real_first(bool real_first);
    
    /* 状态查询函数 */
    
    /**
     * @brief 获取规划器输出（对应RoboWalker的Get_Out）
     * @return float 当前输出值
     */
    inline float Get_out();
    
    /**
     * @brief 获取当前规划值（对应RoboWalker的Now_Planning）
     * @return float 当前规划值
     */
    inline float Get_planning();
    
    /**
     * @brief 获取当前真实值
     * @return float 当前真实值
     */
    inline float Get_real();
    
    /**
     * @brief 获取当前目标值
     * @return float 当前目标值
     */
    inline float Get_target();
    
    /* 控制函数 */
    
    /**
     * @brief 重置斜坡规划器
     */
    void Reset();

};

/* Inline function implementations ------------------------------------------*/

inline void Slope_planner_c::Set_target(float target) {
    this->target = target;
}

inline void Slope_planner_c::Set_increase_value(float inc) {
    this->increase_value = inc;
}

inline void Slope_planner_c::Set_decrease_value(float dec) {
    this->decrease_value = dec;
}

inline void Slope_planner_c::Set_now_real(float real_value) {
    this->now_real = real_value;
}

inline void Slope_planner_c::Set_real_first(bool real_first) {
    this->real_first = real_first;
}

inline float Slope_planner_c::Get_out() {
    return this->out;
}

inline float Slope_planner_c::Get_planning() {
    return this->now_planning;
}

inline float Slope_planner_c::Get_real() {
    return this->now_real;
}

inline float Slope_planner_c::Get_target() {
    return this->target;
}

#endif /* SLOPE_PLANNER_HPP */

/************************ COPYRIGHT(C) SENTRY TEAM **************************/
