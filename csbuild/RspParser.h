#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <map>
using namespace std;

class RspParser {
public:
	void parseRsp(string input);
	void printResult();
private:
	map<string, string> mymap;
};

void RspParser::parseRsp(string input) {
	int errorReportIndex = input.find("/errorReport");
	string flag = input.substr(0, errorReportIndex);
	string sources = input.substr(errorReportIndex);

    std::istringstream iss1(sources);
    std::string token;
	std::getline(iss1, token, ' ');
	flag.append(token);
	sources = sources.substr(token.length()+1);

	std::istringstream iss2(sources);
    while (std::getline(iss2, token, ' '))
    {
		mymap.insert(std::pair<string, string>(token, flag));
    }
}

void RspParser::printResult() {
	// show content:
	for (std::map<string, string>::iterator it=mymap.begin(); it!=mymap.end(); ++it) {
		cout<<"sources : "<<it->first <<endl
			<<"flag : "<<it->second<<endl;
	}
	
}