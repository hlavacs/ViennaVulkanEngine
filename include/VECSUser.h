#ifndef VECSUSER_H
#define VECSUSER_H


#include <limits>
#include <typeinfo>
#include <typeindex>
#include "VECSUtil.h"

namespace vecs {

	//-------------------------------------------------------------------------
	//define user components here

	//declare your own entity components
	struct VeComponentUser1 {
		//..
	};
	//...

	//using VeComponentTypeListUser = tl::type_list<VeComponentUser1>; //include all user components into this list
	
	using VeComponentTypeListUser = vtl::type_list<>; //default is no user define components


	//-------------------------------------------------------------------------
	//define user entity types here

	using VeEntityUser1 = VeEntityType<VeComponentPosition, VeComponentUser1>; //can be any mix of component types

	//using VeEntityTypeListUser = tl::type_list<
	//	VeEntityeUser1
	//  , ...  
	//>;

	using VeEntityTypeListUser = vtl::type_list<>; //default is no user entity types 

}

#endif

