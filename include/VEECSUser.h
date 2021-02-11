#include <limits>
#include <typeinfo>
#include <typeindex>
#include "VEUtil.h"

namespace vve {


	//declare your own entity components
	struct UserComponent1 : VeComponent {
		//..
	};
	//...

	//using VeComponentTypeListUser = tl::type_list<UserComponent1>; //include all into this list
	using VeComponentTypeListUser = tl::type_list<>; //default is no user define components


	//using VeEntityTypeListUser = tl::type_list<
	//	VeEntityType<VeComponentPosition, VeComponentOrientation, VeComponentTransform, UserComponent1>
	//>;
	using VeEntityTypeListUser = tl::type_list<>; //default is empty

}


