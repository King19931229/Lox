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

	// ֧��printf����ʽ��
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

// �޸ĺ󣺸��� baseName ���� Visitor �ӿں�ģ����Ĵ���
std::string GenerateVisitorBody(const std::string& baseName, const std::vector<std::string>& types)
{
	ContentWriter writer;
	
	// ���� baseName ��������߽ӿڵ�����
	std::string iVisitorName = "I" + baseName + "Visitor";
	std::string visitorName = baseName + "Visitor";
	std::string visitMethodName = "Visit" + baseName; // ���������� Visit ������

	// 1. �������������ǰ������
	for (const std::string& type : types)
	{
		std::string className = Util::Trim(type.substr(0, type.find(':')));
		writer.WriteLine("struct %s;", className.c_str());
	}
	writer.WriteLine("struct %s;", baseName.c_str());
	writer.WriteLine("");

	// 2. ���� I<BaseName>Visitor �ӿ�
	writer.WriteLine("struct %s", iVisitorName.c_str());
	writer.EnterScope();
	writer.WriteLine("virtual ~%s() = default;", iVisitorName.c_str());
	for (const std::string& type : types)
	{
		std::string className = Util::Trim(type.substr(0, type.find(':')));
		// Visit ������Ҳ���� baseName ��׺
		writer.WriteLine("virtual void Visit%s%s(const %s* %s) = 0;", className.c_str(), baseName.c_str(), className.c_str(), baseName.c_str());
	}
	writer.ExitDefineScope();
	writer.WriteLine("");

	// 3. ���� <BaseName>Visitor<R> ģ����
	writer.WriteLine("template<typename R>");
	writer.WriteLine("struct %s : public %s", visitorName.c_str(), iVisitorName.c_str());
	writer.EnterScope();
	writer.WriteLine("R result; // ���ڴ洢���ʽ��");
	writer.WriteLine("");
	// ʹ�ö�̬���ɵ� Visit ������
	writer.WriteLine("R %s(const %s* %s);", visitMethodName.c_str(), baseName.c_str(), baseName.c_str());
	writer.WriteLine("");

	// ���� Visit... ������ override
	for (const std::string& type : types)
	{
		std::string className = Util::Trim(type.substr(0, type.find(':')));
		writer.WriteLine("void Visit%s%s(const %s* %s) override { result = DoVisit%s%s(%s); }",
			className.c_str(), baseName.c_str(), className.c_str(), baseName.c_str(), className.c_str(), baseName.c_str(), baseName.c_str());
	}
	writer.WriteLine("");
	writer.WriteLine("protected:");

	// ���ɴ���� DoVisit... ����
	for (const std::string& type : types)
	{
		std::string className = Util::Trim(type.substr(0, type.find(':')));
		writer.WriteLine("virtual R DoVisit%s%s(const %s* %s) = 0;", className.c_str(), baseName.c_str(), className.c_str(), baseName.c_str());
	}
	writer.ExitDefineScope();

	return writer.GetResult();
}

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

	// --- ��Ҫ�޸ģ����ɲ��滻����ռλ�� ---

	// 1. ���� Visitor ����
	std::string visitorBody = GenerateVisitorBody(baseName, types);
	size_t visitorPos = contents.find("$(VISITOR_DEFINE_BODY)");
	if (visitorPos != std::string::npos)
	{
		contents.replace(visitorPos, sizeof("$(VISITOR_DEFINE_BODY)") - 1, visitorBody);
	}

	// 2. ���� Expr �ṹ�嶨��
	std::string exprBody;
	for (const std::string& type : types)
	{
		size_t colonPos = type.find(':');
		if (colonPos == std::string::npos) continue;
		std::string className = Util::Trim(type.substr(0, colonPos));
		std::string fields = Util::Trim(type.substr(colonPos + 1));

		exprBody += DefineType(baseName, className, fields);
	}

	size_t exprPos = contents.find("$(DEFINE_BODY)");
	if (exprPos != std::string::npos)
	{
		contents.replace(exprPos, sizeof("$(DEFINE_BODY)") - 1, exprBody);
	}

	// --- �޸Ľ��� ---

	std::string outputPath = outputDir + "/" + baseName + ".h";
	if (fopen_s(&file, outputPath.c_str(), "wb") != 0 || !file)
	{
		printf("Could not open file: %s\n", templatePath.c_str());
		return;
	}

	fwrite(contents.data(), 1, contents.size(), file);
	fclose(file);
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

	// ���Ϊ���ͺͱ������������ɳ�Ա����
	std::vector<std::string> typeList;
	std::vector<std::string> nameList;

	for (const std::string& field : fieldArray)
	{
		size_t sep = field.find_first_of(" \t");
		if (sep == std::string::npos) continue;

		std::string typePart = Util::Trim(field.substr(0, sep));
		std::string namePart = Util::Trim(field.substr(sep + 1));
		if (typePart.empty() || namePart.empty()) continue;

		// ��Ա���ͣ�Token/Lexeme ����ԭ������������׷�� Ptr
		std::string memberType = (typePart == "Token" || typePart == "Lexeme") ? typePart : (typePart + "Ptr");

		typeList.push_back(memberType);
		nameList.push_back(namePart);

		writer.WriteLine("%s %s;", memberType.c_str(), namePart.c_str());
	}

	// ���캯������������ in ǰ׺���������ڸ�ֵ����ʹ�ó�Ա��ʼ���б�
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
		std::string assignList; // ���ڹ��캯���帳ֵ
		std::string argList;    // ���� Create ���ù��캯��

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

		// ���캯��ǩ��
		writer.WriteLine("%s(%s)", className.c_str(), paramList.c_str());
		writer.EnterScope();
		for (size_t i = 0; i < nameList.size(); ++i)
		{
			const std::string inName = MakeInName(nameList[i]);
			writer.WriteLine("this->%s = %s;", nameList[i].c_str(), inName.c_str());
		}
		writer.ExitScope();

		// ���ɾ�̬�������� Create
		writer.WriteLine("");
		writer.WriteLine("static %sPtr Create(%s)", className.c_str(), paramList.c_str());
		writer.EnterScope();
		writer.WriteLine("return std::make_shared<%s>(%s);", className.c_str(), argList.c_str());
		writer.ExitScope();
	}
	else
	{
		// ���ֶ�������Ĭ�Ϲ������޲� Create
		writer.WriteLine("%s() {}", className.c_str());
		writer.WriteLine("");
		writer.WriteLine("static %sPtr Create()", className.c_str());
		writer.EnterScope();
		writer.WriteLine("return std::make_shared<%s>();", className.c_str());
		writer.ExitScope();
	}

	writer.WriteLine("");
	writer.WriteLine("void Accept(I%sVisitor& visitor) const override", baseName.c_str()); // ע������� I<BaseName>Visitor
	writer.EnterScope();
	writer.WriteLine("visitor.Visit%s%s(this);", className.c_str(), baseName.c_str());
	writer.ExitScope();

	writer.ExitDefineScope();
	return writer.GetResult();
}