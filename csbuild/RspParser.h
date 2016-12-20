#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
using namespace std;

typedef struct dir {
	string workingDir;
	string commonCompiler;
	map<string, string> compileFlag;
} Dir;

class RspParser {
public:
	void parseClexe(string workingDir, string commonCompiler, string rspContents);
	void parseLinkexe(string workingDir, string rspContents);
private:
	std::vector<Dir> dirList;
	string getProjectName(string rspContents);
	void saveToMap(string rspContents, map<string, string> *map);
	void printResult(Dir matchingDir, string rspContents);
};

void RspParser::parseClexe(string workingDir, string commonCompiler, string rspContents) {
	//first directory
	if (dirList.size() == 0) {
		//make new directory
		Dir dir;
		dir.workingDir = workingDir;
		dir.commonCompiler = commonCompiler.substr(0, commonCompiler.find('@'));
		dirList.push_back(dir);
	} 

	//find dir in dirList
	int i = 0;
	for (i = 0; i < dirList.size(); i++) {
		if (dirList[i].workingDir == workingDir) {
			//save in map
			saveToMap(rspContents, &dirList[i].compileFlag);
			return;
		}
	}

	//new directory
	Dir dir;
	dir.workingDir = workingDir;
	dir.commonCompiler = commonCompiler.substr(0, commonCompiler.find('@'));
	dirList.push_back(dir);
	saveToMap(rspContents, &dirList[i].compileFlag);
	return;
}

void RspParser::parseLinkexe(string workingDir, string rspContents) {
	if (dirList.size() == 0) {
		return;
	} 

	//find dir in dirList
	for (int i = 0; i < dirList.size(); i++) {
		if (dirList[i].workingDir == workingDir) {
			printResult(dirList[i], rspContents);
		}
	}
}

void RspParser::saveToMap(string rspContents, map<string, string> *map) {
	int errorReportIndex = rspContents.find("/errorReport");
	string flag = rspContents.substr(0, errorReportIndex);
	string sources = rspContents.substr(errorReportIndex);

	std::istringstream iss1(sources);
	std::string token;
	std::getline(iss1, token, ' ');
	flag.append(token);
	sources = sources.substr(token.length()+1);

	std::istringstream iss2(sources);
	while (std::getline(iss2, token, ' '))
	{
		(*map).insert(std::pair<string, string>(token, flag));
	}
}

string RspParser::getProjectName(string rspContents) {
	string name = "";
	int exeStartIndex = rspContents.find(".exe");

	int index = exeStartIndex - 1;
	while (rspContents[index] != '\\' && index >= 0) {
		name = rspContents[index] + name;
		index--;
	}

	return name;
}

void RspParser::printResult(Dir matchingDir, string rspContents) {
	// show content;
	cout<<"<Project>"<<endl
		<<"\t"<<"<ProjectName> "<< getProjectName(rspContents) <<" </ProjectName>"<<endl
		<<"\t"<<"<BuildLocation> "<<matchingDir.workingDir<<" </BuildLocation>"<<endl
		<<"\t"<<"<LinkFlags> "<<rspContents<<" </LinkFlags>"<<endl
		<<"\t"<<"<CommonCompiler> "<< matchingDir.commonCompiler << " </CommonCompiler>"<<endl;

	std::map<string, string>::iterator it;
	cout<<"\t"<<"<Sources>"<<endl;
	for (it=matchingDir.compileFlag.begin(); it!=matchingDir.compileFlag.end(); ++it) {
		cout<<"\t\t"<<"<Source name="<<it->first <<" compileflag="<<it->second<< " />" <<endl;
	}
	cout<<"\t"<<"</Sources>"<<endl
	<<"</Project>"<<endl;
	
}
