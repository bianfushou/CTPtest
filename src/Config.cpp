#include <string>
#include <fstream>
#include <iostream>
#include "Config.h"
using namespace std;

string trim(const string &s) {
	if (s.empty()) {
		return s;
	}

	string newS = s;
	newS.erase(0, s.find_first_not_of(" "));
	newS.erase(newS.find_last_not_of(" ")+1);
	return newS;
}

/*�������ƣ�getConfig()
�������ܣ���ȡ�����ļ�ini����Ӧ�����title��ָ�������ֶ�cfgname��ֵ
����1��string title		�����[***]
����2��string cfgName		������µ������ֶ�
����ֵ�������ļ�ini����Ӧ�����title��ָ�������ֶ�cfgname��ֵ
*/
int getConfig(string title, string cfgName, string& valName)
{
	cfgName = trim(cfgName);
	title = trim(title);
	const char* INIFile = "config.ini";
	ifstream inifile(INIFile);
	if (!inifile.is_open())
	{
		cerr << "Could not open " << INIFile << endl;
		inifile.clear();
		return -1;
	}
	string strtmp, strtitle, strcfgname, returnValue;
	int flag = 0;
	while (getline(inifile, strtmp, '\n'))
	{
		if (strtmp.substr(0, 1) == "#")	continue;	//����ע��		
		if (flag == 0)
		{
			if (strtmp.find(title) != string::npos)
			{
				if (strtmp.substr(0, 1) == "[")
				{
					if (strtmp.find("]") == string::npos) 	break;	//ȱʧ��]���˳�
					strtitle = strtmp.substr(1);
					strtitle = strtitle.erase(strtitle.find("]"));
					if (strtitle == title)		//�ҵ���������ñ�־λΪ1�������Ͳ�������һ���������
					{
						flag = 1;
						continue;
					}
				}
			}
		}
		if (flag == 1)
		{
			if (strtmp.substr(0, 1) == "[")	break;	//���������һ��[]��˵����ǰ������Ӧ�������ֶ�������ϣ�����������
			if (strtmp.find(cfgName) != string::npos)
			{
				if (strtmp.find("=") == string::npos)	break;	//ȱʧ��=���˳�
				strcfgname = strtmp;
				strcfgname = strcfgname.erase(strcfgname.find("="));
				strcfgname = trim(strcfgname);
				
				if (strcfgname == cfgName)		//�ҵ��������Ӧ���ֶκ󣬷���ֵ
				{
					valName = strtmp.substr(strtmp.find("=") + 1);
					valName = trim(valName);
					return 0;
				}
				else continue;
			}
		}
	}
	cout << "�����ļ�����û�ҵ�" << title << "��Ӧ������" << cfgName << "��" << endl;
	return -1;
}