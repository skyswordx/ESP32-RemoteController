/**
 * @file slope_planner.cpp
 * @brief 斜坡规划器C++类实现文件 - 完全按照RoboWalker Class_Slope设计
 * @version 3.0
 * @date 2025-08-26
 * @author GitHub Copilot
 * 
 * @copyright Copyright (c) 2025 SENTRY Team
 * 
 * @note 简洁C++实现，完全模仿中科大RoboWalker的Class_Slope算法
 *       只有一个主要的Update_period函数，去除复杂的状态机和多个更新函数
 */

#include "slope_planner.hpp"

/* Constructor and destructor -----------------------------------------------*/

Slope_planner_c::Slope_planner_c() {
    Init(0.0f, 0.0f, true);
}

Slope_planner_c::Slope_planner_c(float inc, float dec, bool real_first) {
    Init(inc, dec, real_first);
}

/* Public member functions --------------------------------------------------*/

void Slope_planner_c::Init(float inc, float dec, bool real_first) {
    increase_value = inc;
    decrease_value = dec;
    this->real_first = real_first;
    
    // 清零状态变量
    target = 0.0f;
    now_planning = 0.0f;
    now_real = 0.0f;
    out = 0.0f;
}

/**
 * @brief 斜坡规划器周期更新函数（完全按照RoboWalker算法实现）
 * 
 * @note 完全按照设计文档6.2.1节的思想实现：
 *       1. Out初始化为Now_Planning（或在真实值优先时设为Now_Real）
 *       2. 后续分段逻辑基于Out值进行计算，实现连续的斜坡规划
 *       3. 真实值优先的设计思想：以真实值为新起点，继续朝目标规划
 *       
 *       示例：Target=2.0, Now_Planning=1.5, Now_Real=1.7
 *       - 真实值优先：Out=1.7 → Out=1.7+0.1=1.8 (既响应了真实值，又继续规划)
 */
void Slope_planner_c::Update_period() {
    
    // 规划为当前真实值优先的额外逻辑（完全按照RoboWalker实现）
    if (real_first) {
        if ((target >= now_real && now_real >= now_planning) || 
            (target <= now_real && now_real <= now_planning)) {
            out = now_real;  // 以真实值作为新的起点
            // 注意：这里不返回，让后续逻辑在真实值基础上继续规划
        }
    }

    // 分段规划逻辑，基于now_planning值进行计算
    if (now_planning > 0.0f) {
        if (target > now_planning) {
            // 正值加速
            if (Math_abs(now_planning - target) > increase_value) {
                out += increase_value;
            } else {
                out = target;
            }
        } else if (target < now_planning) {
            // 正值减速
            if (Math_abs(now_planning - target) > decrease_value) {
                out -= decrease_value;
            } else {
                out = target;
            }
        }
    } else if (now_planning < 0.0f) {
        if (target < now_planning) {
            // 负值加速
            if (Math_abs(now_planning - target) > increase_value) {
                out -= increase_value;
            } else {
                out = target;
            }
        } else if (target > now_planning) {
            // 负值减速
            if (Math_abs(now_planning - target) > decrease_value) {
                out += decrease_value;
            } else {
                out = target;
            }
        }
    } else {
        if (target > now_planning) {
            // 0值正加速
            if (Math_abs(now_planning - target) > increase_value) {
                out += increase_value;
            } else {
                out = target;
            }
        } else if (target < now_planning) {
            // 0值负加速
            if (Math_abs(now_planning - target) > increase_value) {
                out -= increase_value;
            } else {
                out = target;
            }
        }
    }
    
    // 善后工作
    now_planning = out;
}

void Slope_planner_c::Reset() {
    target = 0.0f;
    now_planning = 0.0f;
    now_real = 0.0f;
    out = 0.0f;
}

/* Static test function -----------------------------------------------------*/

