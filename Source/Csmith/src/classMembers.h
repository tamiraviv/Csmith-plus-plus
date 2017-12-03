#include <string>
#include "Type.h"


class classMembers
{
private:
	std::string nameOfMember;
	Type* originallyFrom;
	bool isInheritedVirtual;
	bool ishiding;
	std::string hideingFrom;
public:
	classMembers(std::string _nameOfMember, bool _isInheritedVirtual, std::string _originallyFrom, bool _ishiding, std::string _hideingFrom){
		nameOfMember = _nameOfMember;
		isInheritedVirtual = _isInheritedVirtual;
		ishiding = _ishiding;
		hideingFrom = _hideingFrom;
	}
		
	std::string getNameOfMember()
	{
		return nameOfMember;
	}

	bool getIsInheritedVirtual()
	{
		return isInheritedVirtual;
	}

	Type* getOriginallyFrom()
	{
		return originallyFrom;
	}

	bool getIshiding()
	{
		return ishiding;
	}

	std::string getHideingFrom()
	{
		return hideingFrom;
	}
};