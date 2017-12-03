#include <vector>
#include <map>
#include <string>

#include "classMembers.h"



class classesLookupMemberMap
{
private:
	std::map<std::string, std::vector<classMembers>> membersInClass;
public:
	std::vector<classMembers> getMemberInClass(std::string className)
	{
		return membersInClass[className];
	}

};