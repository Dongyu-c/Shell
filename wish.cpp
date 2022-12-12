#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <fcntl.h>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
using namespace std;

//add slash for the address
void addSlash(string& a) {
	if (a.front() != '/') {
		a = '/' + a;
	}
	if (a.back() != '/') {
		a = a + '/';
	}

}

//ptint the error message
void printErr() {
	cerr << "An error has occurred" << endl;
}

//what to do when encount exit command
void Exit(vector<string> a) {
	if (a[0] == "exit") {
		if (a.size() > 1) {
			printErr();
		}
		else {
			exit(0);
		}
	}
}

//what to do when encount change directory command
void Cd(vector<string>& a) {
	if (a[0] == "cd") {
		if (a.size() != 2) {
			printErr();
		}
		else {
			int b = chdir((char*)a[1].c_str());
			if (b != 0) {
				printErr();
			}
		}
	}
}

//what to do when encount path command
void Path(vector<string>& a, vector<string>& b) {
	if (a[0] == "path") {
		b.clear();
		b.push_back("");
		for (size_t i = 1; i < a.size(); i++) {
			char buf[2048];
			getcwd(buf, 2048);
			addSlash(a[i]);
			a[i] = buf + a[i];
			b.push_back(a[i]);
		}
	}


}

// implement the command on path; using execv() to handle error
void execute(vector<string>& command, vector<string>& path) {

	char* argument[100];

	for (size_t i = 0; i < command.size(); i++) {

		argument[i] = new char[100];
		strcpy(argument[i], command[i].c_str());
	}
	argument[command.size()] = NULL;
	for (size_t i = 0; i < path.size(); i++) {
		char commandPath[100];
		strcpy(commandPath, (path[i] + command[0]).c_str());
		execv(commandPath, argument);

	}
	printErr();
	exit(1);
}





//generate the command line and its working mode between build-in/null, redirection and parallel
void modeGenerate(vector<vector<vector<string>>>& command, vector<string> line) {
	command.clear();

	vector<string> lineProcess;
	vector<vector<string>> commandList;

	for (size_t i = 0; i < line.size(); i++) {
		string line_i = line[i];
		line_i.erase(remove_if(line_i.begin(), line_i.end(), [](char x) {return isspace(x); }), line_i.end());
		size_t findRedirect = line_i.find(">");
		size_t findAnd = line_i.find("&");
		//find ">"s and "&"s and split them into storage
		while (findRedirect != string::npos || findAnd != string::npos) {
			if (findAnd == string::npos || findRedirect < findAnd) {
				string redirect1 = line_i.substr(0, findRedirect);


				if (!redirect1.empty()) {
					lineProcess.push_back(redirect1);
				}

				lineProcess.push_back(">");
				line_i = line_i.substr(findRedirect + 1, line_i.size());
				findRedirect = line_i.find(">");
				findAnd = line_i.find("&");
			}
			else {
				string para1 = line_i.substr(0, findAnd);


				if (!para1.empty()) {
					lineProcess.push_back(para1);
				}

				lineProcess.push_back("&");
				line_i = line_i.substr(findAnd + 1, line_i.size());
				findRedirect = line_i.find(">");
				findAnd = line_i.find("&");
			}

		}


		if (!line_i.empty()) {
			lineProcess.push_back(line_i);
		}

	}

	//gather the commands into lines and finally into generaated command as vector
	vector<string>::iterator preChar = lineProcess.begin();
	vector<string>::iterator presentChar = lineProcess.begin();
	while (presentChar != lineProcess.end()) {
		if (*presentChar == "&" || *presentChar == ">") {

			if (preChar != presentChar) {
				vector<string> commandIn(preChar, presentChar);
				commandList.push_back(commandIn);
			}
			preChar = presentChar + 1;

			vector<string> bindChar(presentChar, presentChar + 1);
			commandList.push_back(bindChar);
			
		}
		presentChar++;
	}
	if (preChar != lineProcess.end()) {
		vector<string> commandIn(preChar, presentChar);
		commandList.push_back(commandIn);
	}


	vector<vector<string>>::iterator preCommand = commandList.begin();
	vector<vector<string>>::iterator currentCommand = commandList.begin();
	while (currentCommand != commandList.end()) {
		if ((*currentCommand).front() == "&") {
			vector<vector<string>> bindCommand(preCommand, currentCommand);
			command.push_back(bindCommand);
			preCommand = currentCommand + 1;
		}
		currentCommand++;
	}
	vector<vector<string>> bindCommand(preCommand, commandList.end());
	command.push_back(bindCommand);




}

//count the num till NULL using wait()
void waitCount(int& count) {
	while (count != 0) {
		wait(NULL);
		count -= 1;
	}
}


//run the command accoring the path given, involving child if required
void convertCommand(vector<string>& inputCommand, vector<string>& path) {


	if (inputCommand.size() != 0) {
		//3 built-in commands
		Exit(inputCommand);
		Cd(inputCommand);
		Path(inputCommand, path);
		//if no command requirements
		if ((inputCommand[0] != "exit") && (inputCommand[0] != "cd") && (inputCommand[0] != "path")) {


			vector<vector<vector<string>>> generatedCommand;

			modeGenerate(generatedCommand, inputCommand);

			int countChildNum;
			for (auto subCommand : generatedCommand) {
				if (subCommand.empty()) {
					continue;
				}
				else {
					//set child
					pid_t pid = fork();

					if (pid == 0) {
						if (subCommand.size() == 1) {
							execute(subCommand[0], path);
							printErr();
							exit(0);
						}
						else if(subCommand.size() == 3) {
							if (subCommand[2].size() == 1) {
								int fd;
								fd = open(subCommand[2][0].c_str(), O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IXUSR);
								dup2(fd, 1);
								dup2(fd, 2);
								close(fd);
								execute(subCommand[0], path);
							}
							printErr();
							exit(0);
						}
						else {
							printErr();
							exit(0);
						}
					}

					else {
						countChildNum++;
					}
				}
			}
			//wait the child cout till the end
			waitCount(countChildNum);

		}
	}
}


int main(int argc, char* argv[]) {

	vector<string> inputCommand;
	vector<string> path;

	string inputLine;

	path.push_back("/bin/");

	path.insert(path.begin(), "");
	//interactive mode
	if (argc == 1) {
		while (getline(cin, inputLine)) {
			cout << "wish> ";

			inputCommand.clear();

			string a;
			istringstream inputStream(inputLine);


			while (inputStream >> a) {
				inputCommand.push_back(a);
			}

			inputStream.clear();

			convertCommand(inputCommand, path);

		}
	}
	//batch mode
	else if (argc == 2) {

		ifstream batch(argv[1]);
		if (batch.fail()) {
			printErr();
			exit(1);
		}
		while (getline(batch, inputLine)) {

			string a;
			istringstream inputStream(inputLine);
			inputCommand.clear();

			while (inputStream >> a) {
				inputCommand.push_back(a);
			}

			inputStream.clear();
			convertCommand(inputCommand, path);
		}
		batch.close();
	}
	//error condition
	else {
		printErr();
		exit(1);
	}
	// no error, exit with 0
	exit(0);
}
