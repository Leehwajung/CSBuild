#ifndef __GCCPARSER_H__
#define __GCCPARSER_H__

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <map>
using namespace std;

class GccParser
{
public:
	GccParser();
	~GccParser();
	
	/* compile infos getter, setter */
	const string getProjectName() const { return projectName; }
	const string getLinkFlag() const { return linkFlags; }
	const string getBuildLocation() const { return buildLocation; }
	const string getCommonCompiler() const { return commonCompiler; }
	
	void setProjectName(const string& projectName) { this->projectName = projectName;  }
	void setLinkFlag(const string& linkFlags) { this->linkFlags = linkFlags; }
	void setBuildLocation(const string& buildLocation) { this->buildLocation = buildLocation; }
	void setCommonCompiler(const string& commonCompiler) { this->commonCompiler = commonCompiler; }
	
	/* add, export compile infos */
	void addCompileInfo(const string& rareCompileInfo);
	void printCompileInfos() const;
	void setOutputFile(const string& fileName) { this->outputFileName = fileName; }
	const string getOutputFileName() const { return outputFileName; }
	void exportFCompileInfos() const;

private:
	string toUpperCase(const string& str);

	bool isCompilerCall(const string& command);
	bool isSourceFiie(const string& command);
	bool isObjectFiie(const string& command);
	bool isExecutableFiie(const string& command);

	string extractProjectName(const string& str);
	vector<string> split(const string& str, const char& delim);

	string projectName;
	string linkFlags;
	string buildLocation;
	string commonCompiler;

	/* map sourcefiles into flags */
	map<string, string> sourceFiles;
	vector<string> compileFlags;

	ofstream fout;
	string outputFileName;
};

#endif /* __GCCPARSER_H__ */