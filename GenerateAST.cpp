#include "GenerateAST.h"
#include "Util.h"
#include <cstdio>
#include <cstdarg>
#include <string>
#include <vector>

class ContentWriter
{
public:
	ContentWriter() : indentLevel(0), indentStr("\t") {}

	// 支持printf风格格式化
	void WriteLine(const char* fmt, ...)
	{
		char buffer[1024];
		va_list args;
		va_start(args, fmt);
		std::vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);

		for (int i = 0; i < indentLevel; ++i)
			result += indentStr;
		result += buffer;
		result += "\n";
	}

	void EnterScope()
	{
		WriteLine("{");
		++indentLevel;
	}

	void ExitScope()
	{
		if (indentLevel > 0)
			--indentLevel;
		WriteLine("}");
	}

	void ExitDefineScope()
	{
		if (indentLevel > 0)
			--indentLevel;
		WriteLine("};");
	}

	const std::string& GetResult() const
	{
		return result;
	}

private:
	std::string result;
	int indentLevel;
	std::string indentStr;
};

void GenerateAST::DefineAST(const std::string& outputDir, const std::string& baseName, const std::vector<std::string>& types)
{
	std::string templatePath = outputDir + "/" + baseName + ".template.h";
	FILE* file = nullptr;
	if (fopen_s(&file, templatePath.c_str(), "rb") != 0 || !file)
	{
		printf("Could not open file: %s\n", templatePath.c_str());
		return;
	}

	fseek(file, 0, SEEK_END);
	size_t size = static_cast<size_t>(ftell(file));
	fseek(file, 0, SEEK_SET);

	std::string contents(size, '\0');
	fread(&contents[0], 1, size, file);
	fclose(file);

	std::string defineBody;
	for (const std::string& type : types)
	{
		size_t colonPos = type.find(':');
		if (colonPos == std::string::npos) continue;
		std::string className = type.substr(0, colonPos);
		std::string fields = type.substr(colonPos + 1);

		className.erase(0, className.find_first_not_of(" \t\r\n"));
		className.erase(className.find_last_not_of(" \t\r\n") + 1);
		fields.erase(0, fields.find_first_not_of(" \t\r\n"));
		fields.erase(fields.find_last_not_of(" \t\r\n") + 1);

		defineBody += DefineType(baseName, className, fields);
	}

	size_t pos = 0;
	const std::string placeholder = "$(EXPR_DEFINE_BODY)";
	while ((pos = contents.find(placeholder, pos)) != std::string::npos)
	{
		contents.replace(pos, placeholder.length(), defineBody);
		pos += defineBody.length();
	}

	std::string outputPath = outputDir + "/" + baseName + ".h";
	if (fopen_s(&file, outputPath.c_str(), "wb") != 0 || !file)
	{
		printf("Could not open file: %s\n", templatePath.c_str());
		return;
	}

	fwrite(contents.data(), 1, contents.size(), file);
}


std::string GenerateAST::DefineType(const std::string& baseName, const std::string& className, const std::string& fields)
{
	ContentWriter writer;
	writer.WriteLine("struct %s;", className.c_str());
	writer.WriteLine("typedef std::shared_ptr<%s> %sPtr;", className.c_str(), className.c_str());
	writer.WriteLine("");
	writer.WriteLine("struct %s : public %s", className.c_str(), baseName.c_str());
	writer.EnterScope();

	std::vector<std::string> fieldArray = Util::SplitFields(fields);

	// 拆分为类型和变量名，并生成成员声明
	std::vector<std::string> typeList;
	std::vector<std::string> nameList;

	for (const std::string& field : fieldArray)
	{
		size_t sep = field.find_first_of(" \t");
		if (sep == std::string::npos) continue;

		std::string typePart = Util::Trim(field.substr(0, sep));
		std::string namePart = Util::Trim(field.substr(sep + 1));
		if (typePart.empty() || namePart.empty()) continue;

		// 成员类型：Token/Lexeme 保持原样，其它类型追加 Ptr
		std::string memberType = (typePart == "Token" || typePart == "Lexeme") ? typePart : (typePart + "Ptr");

		typeList.push_back(memberType);
		nameList.push_back(namePart);

		writer.WriteLine("%s %s;", memberType.c_str(), namePart.c_str());
	}

	// 构造函数：参数名加 in 前缀，函数体内赋值（不使用成员初始化列表）
	writer.WriteLine("");
	if (!nameList.empty())
	{
		auto MakeInName = [](const std::string& n) {
			std::string res = "in";
			if (!n.empty())
			{
				char c0 = n[0];
				if (c0 >= 'a' && c0 <= 'z') res += static_cast<char>(c0 - 'a' + 'A');
				else res += c0;
				if (n.size() > 1) res += n.substr(1);
			}
			return res;
		};

		std::string paramList;
		std::string assignList; // 用于构造函数体赋值
		std::string argList;    // 用于 Create 调用构造函数

		for (size_t i = 0; i < nameList.size(); ++i)
		{
			const std::string inName = MakeInName(nameList[i]);

			if (i > 0) {
				paramList += ", ";
				argList += ", ";
			}

			paramList += "const " + typeList[i] + "& " + inName;
			argList += inName;
		}

		// 构造函数签名
		writer.WriteLine("%s(%s)", className.c_str(), paramList.c_str());
		writer.EnterScope();
		for (size_t i = 0; i < nameList.size(); ++i)
		{
			const std::string inName = MakeInName(nameList[i]);
			writer.WriteLine("this->%s = %s;", nameList[i].c_str(), inName.c_str());
		}
		writer.ExitScope();

		// 生成静态工厂方法 Create
		writer.WriteLine("");
		writer.WriteLine("static %sPtr Create(%s)", className.c_str(), paramList.c_str());
		writer.EnterScope();
		writer.WriteLine("return std::make_shared<%s>(%s);", className.c_str(), argList.c_str());
		writer.ExitScope();
	}
	else
	{
		// 无字段则生成默认构造与无参 Create
		writer.WriteLine("%s() {}", className.c_str());
		writer.WriteLine("");
		writer.WriteLine("static %sPtr Create()", className.c_str());
		writer.EnterScope();
		writer.WriteLine("return std::make_shared<%s>();", className.c_str());
		writer.ExitScope();
	}

	writer.ExitDefineScope();
	return writer.GetResult();
}