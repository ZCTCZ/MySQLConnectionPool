#pragma once

#include <string>
#include <cstdint>

/**
 * 这个类里面封装了数据类型
 */
struct Param 
{
	enum Type 
	{
		STRING,   // 对应CHAR和VARCHAR
		UINT32,   // 对应 INT UNSIGNED
		ENUM_STR  // 对应ENUM，本质是字符串，但语义上可单独标记（实际仍用 STRING）
	} type;

	std::string str_val;
	uint32_t uint_val;

	// 静态工厂函数
	static Param String(const std::string& s) 
	{
		Param p; 
		p.type = STRING; 
		p.str_val = s; 
		return p;
	}
	// 静态工厂函数
	static Param UInt32(uint32_t u) 
	{
		Param p; 
		p.type = UINT32; 
		p.uint_val = u; 
		return p;
	}

	// 静态工厂函数
	// ENUM 本质是字符串，所以用 String 即可，但为清晰也可单独封装
	static Param Enum(const std::string& e) 
	{
		return String(e); // ENUM 在绑定时仍用 STRING
	}
};