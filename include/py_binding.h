#ifndef PY_BINDING_H
#define PY_BINDING_H

#include <string>
// 引入解释器头文件
#include "interpreter.h"

/**
 * @brief 解析DSL文件并导出极简JSON字符串（供Python读取核心信息）
 * @param dsl_file_path DSL文件路径
 * @return JSON字符串（包含game_name/roles_count/has_error）
 * @throw 若文件读取/解析失败，抛出std::runtime_error
 */
std::string parse_dsl_to_json(const std::string& dsl_file_path);

/**
 * @brief 执行DSL文件并返回核心执行日志
 * @param dsl_file_path DSL文件路径
 * @return 执行日志（仅包含核心流程+错误信息）
 * @throw 若解析/执行失败，抛出std::runtime_error
 */
std::string run_dsl(const std::string& dsl_file_path);

/**
 * @brief 初始化Python绑定（供PyBind11注册核心接口）
 */
void init_python_binding();

#endif // PY_BINDING_H