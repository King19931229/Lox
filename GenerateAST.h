#pragma once
#include <string>
#include <vector>

struct GenerateAST
{
	static void DefineAST(const std::string& outputDir, const std::string& baseName, const std::vector<std::string>& types);
	static std::string DefineType(const std::string& baseName, const std::string& className, const std::string& fields);
};
