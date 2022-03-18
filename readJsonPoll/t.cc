#include <iostream>
#include <string>
#include <cstdlib>
#include <cstdio>

#include "json.hpp"

using namespace std;

int main()
{
	nlohmann::json jsdic; 
	jsdic["type"] = 1;
	jsdic["msg"] = "hello world";

	string msg = jsdic.dump();
	cout<<msg<<endl;

	char buf[1024] = {0};
//	char buf[1024] ;
	snprintf(buf, msg.size()+1, "%s", msg.c_str());
	printf("%s\n",buf);

	memset(buf, 0, sizeof(buf));
	fgets(buf,1000, stdin);
	printf("%s\n",buf);

	return 0;
}
