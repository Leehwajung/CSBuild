#include "GccParser.h"


/********************* public start *************************/

GccParser::GccParser() {
	linkFlags = "linkFlags";
	projectName = "projectName";
	buildLocation = "/BuildLoation\\";
	commonCompiler = "/bin/commonCompiler";
}

GccParser::~GccParser() {

}

void GccParser::addCompileInfo(const string& rareCompileInfo) {
	if (rareCompileInfo.find(".exe") != string::npos) {
		setProjectName( extractProjectName(rareCompileInfo) );
		setLinkFlag(rareCompileInfo);
		printCompileInfos();
		return;
	}

	vector<string> tokens = split(rareCompileInfo, ' ');
	vector<string> sources; sources.clear();
	string compileFlag = "";

	/* split into source files and compile flags */
	for (vector<string>::const_iterator it = tokens.begin(); it != tokens.end(); ++it) {
		if (isCompilerCall(*it)) {
			continue;
		}

		if (isSourceFiie(*it)) {
			sources.push_back(*it);
		}
		//else if (isExecutableFiie(*it)) {

		//}
		else {
			if (!compileFlag.empty()) {
				compileFlag.append(" ");
			}
			compileFlag.append(*it);
		}
	}

	for (int i = 0; i < sources.size(); ++i) {
		string key = sources[i];
		sourceFiles[key] = compileFlag;
	}
}

void GccParser::printCompileInfos() const {
	cout << "<Project>" << endl;
	cout << "\t" << "<ProjectName>" << getProjectName() << "</ProjectName>" << endl;
	cout << "\t" << "<BuildLocation>" << getBuildLocation() << "</BuildLocation>" << endl;
	cout << "\t" << "<LinkFlags>" << getLinkFlag() << "</LinkFlags>" << endl;
	cout << "\t" << "<Sources>" << endl;

	for (map<string, string>::const_iterator it = sourceFiles.begin(); it != sourceFiles.end(); ++it) {
		cout << "\t\t" << "<Source " 
			<< "name=" << "\"" << it->first << "\"" << " "
			<< "compileFlag=" << "\"" << it->second << "\"" << " "
			<< "/>" << endl;
	}

	cout << "\t" << "</Sources>" << endl;
	cout << "</Project>" << endl;
}

void GccParser::exportFCompileInfos() const {
	ofstream fout;
	fout.open(outputFileName);
	for (vector<string>::const_iterator it = compileFlags.begin(); it != compileFlags.end(); ++it) {
		fout << *it << endl;
	}
	fout.close();
}

/********************* public end *************************/






/********************* private start *************************/

string GccParser::toUpperCase(const string& str) {
	string result = "";
	for (int i = 0; i < str.size(); ++i) {
		char ch = str[i];
		if ('a' <= str[i] && str[i] <= 'z') {
			ch = str[i] - 'a' + 'A';
		}
		result += ch;
	}
	return result;
}

bool GccParser::isCompilerCall(const string& command) {
	string toFirstPointFromReverse = "";
	string extension;

	for (string::const_reverse_iterator rit = command.rbegin(); rit != command.rend(); ++rit) {
		if (*rit == '.') {
			break;
		}
		toFirstPointFromReverse = (*rit) + toFirstPointFromReverse;
	}
	extension = toUpperCase(toFirstPointFromReverse);
	if ((extension == "GCC") || (extension == "G++")) {
		return true;
	}

	return false;
}

bool GccParser::isSourceFiie(const string& command) {
	string toFirstPointFromReverse = "";
	string extension;

	for (string::const_reverse_iterator rit = command.rbegin(); rit != command.rend(); ++rit) {
		if (*rit == '.') {
			break;
		}
		toFirstPointFromReverse = (*rit) + toFirstPointFromReverse;
	}
	extension = toUpperCase(toFirstPointFromReverse);
	if ( (extension == "CPP") || (extension == "C") ) {
		return true;
	}

	return false;
}

bool GccParser::isObjectFiie(const string& command) {
	string toFirstPointFromReverse = "";
	string extension;

	for (string::const_reverse_iterator rit = command.rbegin(); rit != command.rend(); ++rit) {
		if (*rit == '.') {
			break;
		}
		toFirstPointFromReverse = (*rit) + toFirstPointFromReverse;
	}
	extension = toUpperCase(toFirstPointFromReverse);
	if ((extension == "O")) {
		return true;
	}

	return false;
}

bool GccParser::isExecutableFiie(const string& command) {
	string toFirstPointFromReverse = "";
	string extension;

	for (string::const_reverse_iterator rit = command.rbegin(); rit != command.rend(); ++rit) {
		if (*rit == '.') {
			break;
		}
		toFirstPointFromReverse = (*rit) + toFirstPointFromReverse;
	}
	extension = toUpperCase(toFirstPointFromReverse);
	if ((extension == "EXE")) {
		return true;
	}

	return false;
}

string GccParser::extractProjectName(const string& str) {
	string name = "";
	int exeStartIndex = str.find(".exe");

	int index = exeStartIndex - 1;
	while (str[index] != ' ' && index >= 0) {
		name = str[index] + name;
		index--;
	}

	return name;
}

vector<string> GccParser::split(const string& str, const char& delim) {
	vector<string> tokens;
	string::const_iterator it = str.begin();

	string substring;
	while (it != str.end()) {
		if (*it != delim) {
			substring.push_back(*it);
		}
		else {
			tokens.push_back(substring);
			substring.clear();
		}
		++it;
	}
	tokens.push_back(substring);

	return tokens;
}

/********************* private end *************************/