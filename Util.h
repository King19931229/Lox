#pragma once
#include <string>
#include <vector>

namespace Util
{
	// 去除首尾空白（空格/Tab/CR/LF）
	inline std::string Trim(const std::string& s)
	{
		size_t start = s.find_first_not_of(" \t\r\n");
		if (start == std::string::npos) return std::string();
		size_t end = s.find_last_not_of(" \t\r\n");
		return s.substr(start, end - start + 1);
	}

	// 按逗号分割，并对每个片段做 Trim，空片段会被忽略
	inline std::vector<std::string> SplitFields(const std::string& fields)
	{
		std::vector<std::string> out;
		size_t start = 0;
		while (start < fields.size())
		{
			size_t end = fields.find(',', start);
			std::string field = (end == std::string::npos)
				? fields.substr(start)
				: fields.substr(start, end - start);

			// 与现有实现一致：仅检查 start!=npos 即可
			size_t s = field.find_first_not_of(" \t\r\n");
			size_t e = field.find_last_not_of(" \t\r\n");
			if (s != std::string::npos)
				field = field.substr(s, e - s + 1);
			else
				field.clear();

			if (!field.empty())
				out.push_back(field);

			if (end == std::string::npos) break;
			start = end + 1;
		}
		return out;
	}
}