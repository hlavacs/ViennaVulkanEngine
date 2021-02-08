#include <limits>
#include <typeinfo>
#include <typeindex>
#include "VEUtil.h"

namespace vve {

	enum class VeComponentTypeUser : uint32_t {
		UserComponentType1 = VeComponentType::Last,
		Last
	};

	//declare your own entity components
	struct UserComponentType1 : VeComponent<UserComponentType1, VeComponentTypeUser::UserComponentType1> {
		//..
	};

	//using VeComponentTypeListUser = tl::type_list<UserComponentType1>; //include all into this list
	using VeComponentTypeListUser = tl::type_list<>;


	//using VeEntityTypeListUser = tl::type_list<
	//	VeEntity<VePosition, VeOrientation, VeTransform, UserComponentType1>
	//>;
	using VeEntityTypeListUser = tl::type_list <>;

}


