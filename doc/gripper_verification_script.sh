#!/bin/bash
# 夹爪控制系统验证脚本
# 使用方法：将以下命令逐行复制到ESP32串口监视器中执行

echo "=== 夹爪控制系统验证脚本 ==="
echo ""
echo "请将以下命令逐行复制到ESP32串口监视器中执行："
echo ""

echo "# 1. 基础功能验证"
echo "servo_gripper_status 1"
echo "# 预期：显示夹爪当前状态信息"
echo ""

echo "# 2. 基础运动测试"
echo "servo_gripper_smooth 1 50"
echo "# 预期：夹爪平滑移动到50%位置"
echo ""
echo "等待3秒..."
echo ""

echo "servo_gripper_status 1"
echo "# 预期：Position显示约50%，State为HOLDING或MOVING"
echo ""

echo "# 3. 精度测试（小步长）"
echo "servo_gripper_smooth 1 55"
echo "# 预期：夹爪从50%移动到55%"
echo ""

echo "servo_gripper_smooth 1 45" 
echo "# 预期：夹爪从55%移动到45%"
echo ""

echo "servo_gripper_status 1"
echo "# 预期：Position显示约45%"
echo ""

echo "# 4. 大范围运动测试"
echo "servo_gripper_smooth 1 100 3000"
echo "# 预期：夹爪3秒内移动到100%（完全张开）"
echo ""
echo "等待4秒..."
echo ""

echo "servo_gripper_smooth 1 0 3000"
echo "# 预期：夹爪3秒内移动到0%（完全闭合）"
echo ""
echo "等待4秒..."
echo ""

echo "# 5. 控制模式测试"
echo "servo_gripper_mode 1 closed_loop"
echo "# 预期：切换到闭环控制模式"
echo ""

echo "servo_gripper_smooth 1 25"
echo "# 预期：使用闭环控制移动到25%"
echo ""

echo "servo_gripper_status 1"
echo "# 预期：Mode显示CLOSED_LOOP，位置误差应较小"
echo ""

echo "# 6. 参数调优测试"
echo "servo_gripper_params 1 1.0 1.0 0.8 0.2 0.1 15.0"
echo "# 预期：更新控制参数"
echo ""

echo "servo_gripper_smooth 1 75"
echo "# 预期：使用新参数移动到75%"
echo ""

echo "# 7. 紧急停止测试"
echo "servo_gripper_smooth 1 10 10000"
echo "# 预期：开始一个10秒的长时间运动"
echo ""
echo "立即执行下一条命令："
echo "servo_gripper_stop 1"
echo "# 预期：立即停止运动"
echo ""

echo "servo_gripper_status 1"
echo "# 预期：is_moving显示NO，State显示HOLDING"
echo ""

echo "# 8. 错误处理测试"
echo "servo_gripper_smooth 1 150"
echo "# 预期：报错 Invalid gripper percent"
echo ""

echo "servo_gripper_smooth 1 50 50"
echo "# 预期：报错 Invalid time"
echo ""

echo "# 9. 映射配置测试"
echo "servo_gripper_config 1 160 90 3"
echo "# 预期：配置角度映射成功"
echo ""

echo "servo_gripper_smooth 1 0"
echo "# 预期：移动到160°位置（闭合）"
echo ""

echo "servo_gripper_smooth 1 100"
echo "# 预期：移动到90°位置（张开）"
echo ""

echo "# 10. 最终状态检查"
echo "servo_gripper_status 1"
echo "# 预期：显示完整的夹爪状态信息"
echo ""

echo "=== 验证完成 ==="
echo ""
echo "如果所有步骤都按预期执行，说明夹爪控制系统工作正常！"
echo ""
echo "关键指标验证："
echo "- 位置精度：Position Error < 5%"
echo "- 响应时间：小变化在2秒内完成"
echo "- 反馈有效性：Feedback显示VALID"
echo "- 运动平滑性：无跳跃，渐进变化"
