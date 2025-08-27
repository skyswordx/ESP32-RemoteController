/**
 * @file pid_controller.cpp
 * @brief 增强型PID控制器C++类实现文件 - 基于RoboWalker Class_PID设计
 * @version 2.0
 * @date 2025-08-25
 * @author GitHub Copilot
 * 
 * @copyright Copyright (c) 2025 SENTRY Team
 * 
 * @note 简洁C++实现，直接将C函数转换为类成员函数，保持完全相同的算法
 */

#include "pid_controller.hpp"
#include <cstring>

/* Private macros ------------------------------------------------------------*/
#define PID_DEFAULT_DT              (0.001f)    // 默认控制周期1ms
#define PID_DEFAULT_DEAD_ZONE       (0.0f)      // 默认无死区
#define PID_EPSILON                 (1e-6f)     // 浮点数比较精度

/* Constructor and destructor -----------------------------------------------*/

PID_controller_c::PID_controller_c() {
    Init(0.0f, 0.0f, 0.0f);
}

PID_controller_c::PID_controller_c(float kp, float ki, float kd) {
    Init(kp, ki, kd);
}

/* Public member functions --------------------------------------------------*/

void PID_controller_c::Init(float kp, float ki, float kd) {
    // 清零结构体
    std::memset(this, 0, sizeof(PID_controller_c));
    
    // 设置基本PID参数
    this->kp = kp;
    this->ki = ki;
    this->kd = kd;
    this->kf = 0.0f;
    
    // 设置默认控制参数
    this->dt = PID_DEFAULT_DT;
    this->dead_zone = PID_DEFAULT_DEAD_ZONE;
    this->output_limit = 0.0f;        // 0表示不限制
    this->integral_limit = 0.0f;      // 0表示不限制
    
    // 变速积分默认关闭
    this->i_variable_speed_a = 0.0f;
    this->i_variable_speed_b = 0.0f;
    
    // 积分分离默认关闭
    this->i_separate_threshold = 0.0f;
    
    // 功能配置
    this->enable_d_first = false;
    this->enable_integral_limit = false;
    this->enable_output_limit = false;
    
    // 初始状态
    this->state = PID_state_c::PID_STATE_STOP;
}

void PID_controller_c::Init_full(float kp, float ki, float kd, float kf,
                             float integral_limit, float output_limit, float dt,
                             float dead_zone, float i_variable_speed_a, float i_variable_speed_b,
                             float i_separate_threshold, bool d_first) {
    // 基础初始化
    Init(kp, ki, kd);
    
    // 设置完整参数
    this->kf = kf;
    this->integral_limit = Math_abs(integral_limit);
    this->output_limit = Math_abs(output_limit);
    this->dt = (dt > PID_EPSILON) ? dt : PID_DEFAULT_DT;
    this->dead_zone = Math_abs(dead_zone);
    this->i_variable_speed_a = Math_abs(i_variable_speed_a);
    this->i_variable_speed_b = Math_abs(i_variable_speed_b);
    this->i_separate_threshold = Math_abs(i_separate_threshold);
    this->enable_d_first = d_first;
    
    // 使能标志
    this->enable_integral_limit = (integral_limit > PID_EPSILON);
    this->enable_output_limit = (output_limit > PID_EPSILON);
}

/**
 * @brief 按照设定的 pid->dt 周期更新控制器状态
 * @return float 输出值
 */
float PID_controller_c::Update_period(float target, float feedback) {
    
    // 清空上一次的输出值
    this->p_out = 0.0f;
    this->i_out = 0.0f;
    this->d_out = 0.0f;
    this->f_out = 0.0f;

    // 更新目标值和反馈值
    this->target = target;
    this->feedback = feedback;
    
    // 计算误差
    float error = target - feedback;
    float abs_error = Math_abs(error);
    
    // 优化1：死区处理内联展开（参考RoboWalker算法）
    if (this->dead_zone >= PID_EPSILON) {
        // 启用死区处理
        if (abs_error <= this->dead_zone) {
            // 在死区内，清零误差
            this->target = this->feedback;
            error = 0.0f;
            abs_error = 0.0f;

        } else if (error > 0.0f && abs_error > this->dead_zone) {
            // 正误差，减去死区
            error -= this->dead_zone;
        } else if (error < 0.0f && abs_error > this->dead_zone) {
            // 负误差，加上死区
            error += this->dead_zone;
        }
    }
    abs_error = Math_abs(error);
    
    // 更新当前误差
    this->error = error;
    
    // 更新最大误差统计
    if (abs_error > this->max_error) {
        this->max_error = abs_error;
    }
    
    // 计算P项输出
    this->p_out = this->kp * error;
    
    // 优化2：变速积分算法内联展开（RoboWalker变速积分算法）
    float integral_ratio;
    if (this->i_variable_speed_a < PID_EPSILON && this->i_variable_speed_b < PID_EPSILON) {
        // 未启用变速积分，完全积分
        integral_ratio = 1.0f;
    } else {
        if (abs_error <= this->i_variable_speed_a) {
            // 定速区：完全积分 abs(error) <= a
            integral_ratio = 1.0f;
        } else if (abs_error < this->i_variable_speed_b) {
            // 变速区：线性衰减 a <= abs(error) <= b
            integral_ratio = (this->i_variable_speed_b - abs_error) / (this->i_variable_speed_b - this->i_variable_speed_a);
        } else {
            // 禁用区：不积分 abs(error) > b
            integral_ratio = 0.0f;
        }
    }

    // 积分限幅（在累加前进行限幅）
    if (this->enable_integral_limit && this->ki > PID_EPSILON) {
        float max_integral = this->integral_limit / this->ki;
        this->integral_error = Math_constrain_value(this->integral_error, -max_integral, max_integral);
    }
    
    // 积分分离判断
    if (this->i_separate_threshold > PID_EPSILON && abs_error >= this->i_separate_threshold) {
        // 在积分分离区间，清零积分项
        this->integral_error = 0.0f;
        this->i_out = 0.0f;
    } else {
        // 不在积分分离区间，正常积分
        this->integral_error += integral_ratio * this->dt * error;
        this->i_out = this->ki * this->integral_error;
    }
    
    // 计算D项输出（RoboWalker微分先行算法）
    if (this->enable_d_first) {
        // 微分先行：对反馈值微分，减少微分冲击
        this->d_out = -this->kd * (feedback - this->pre_feedback) / this->dt;
    } else {
        // 传统微分：对误差微分
        this->d_out = this->kd * (error - this->pre_error) / this->dt;
    }
    
    // 计算前馈输出（RoboWalker前馈算法）
    this->f_out = this->kf * (target - this->pre_target);
    
    // 计算总输出
    this->output = this->p_out + this->i_out + this->d_out + this->f_out;
    
    // 输出限幅
    if (this->enable_output_limit) {
        this->output = Math_constrain_value(this->output, -this->output_limit, this->output_limit);
    }
    
    // 善后工作（保存历史值）
    this->pre_feedback = feedback;
    this->pre_target = target;
    this->pre_error = error;
    this->pre_output = this->output;
    
    // 更新状态
    Update_state();
    
    // 更新计数器
    this->update_count++;
    
    return this->output;
}

void PID_controller_c::Set_params(float kp, float ki, float kd) {
    this->kp = kp;
    this->ki = ki;
    this->kd = kd;
}

void PID_controller_c::Set_variable_integral(float threshold_a, float threshold_b) {
    this->i_variable_speed_a = Math_abs(threshold_a);
    this->i_variable_speed_b = Math_abs(threshold_b);
    
    // 确保B >= A
    if (this->i_variable_speed_b < this->i_variable_speed_a) {
        float temp = this->i_variable_speed_a;
        this->i_variable_speed_a = this->i_variable_speed_b;
        this->i_variable_speed_b = temp;
    }
}

void PID_controller_c::Reset() {
    // 清零状态变量，保留参数配置
    this->target = 0.0f;
    this->feedback = 0.0f;
    this->output = 0.0f;
    this->error = 0.0f;
    this->integral_error = 0.0f;
    
    // 清零历史记录
    this->pre_feedback = 0.0f;
    this->pre_target = 0.0f;
    this->pre_error = 0.0f;
    this->pre_output = 0.0f;
    
    // 清零调试信息
    this->p_out = 0.0f;
    this->i_out = 0.0f;
    this->d_out = 0.0f;
    this->f_out = 0.0f;
    this->max_error = 0.0f;
    this->update_count = 0;
    
    // 重置状态
    this->state = PID_state_c::PID_STATE_STOP;
}

void PID_controller_c::Clear_integral() {
    this->integral_error = 0.0f;
    this->i_out = 0.0f;
}


/* Private member functions -------------------------------------------------*/

void PID_controller_c::Update_state() {
    float abs_error = Math_abs(this->error);
    
    if (abs_error < PID_EPSILON) {
        this->state = PID_state_c::PID_STATE_STOP;
    } else if (abs_error <= this->dead_zone) {
        this->state = PID_state_c::PID_STATE_DEAD_ZONE;
    } else if (this->enable_output_limit && Math_abs(this->output) >= this->output_limit - PID_EPSILON) {
        this->state = PID_state_c::PID_STATE_SATURATED;
    } else {
        this->state = PID_state_c::PID_STATE_NORMAL;
    }
}

/************************ COPYRIGHT(C) SENTRY TEAM **************************/
