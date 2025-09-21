#pragma once
#include <string>
#include <vector>

namespace Util
{
	// ȥ����β�հף��ո�/Tab/CR/LF��
	inline std::string Trim(const std::string& s)
	{
		size_t start = s.find_first_not_of(" \t\r\n");
		if (start == std::string::npos) return std::string();
		size_t end = s.find_last_not_of(" \t\r\n");
		return s.substr(start, end - start + 1);
	}

	// �����ŷָ����ÿ��Ƭ���� Trim����Ƭ�λᱻ����
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

			// ������ʵ��һ�£������ start!=npos ����
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