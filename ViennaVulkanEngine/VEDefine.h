#ifndef VEDEFINE_H
#define VEDEFINE_H


//----------------------------------------------------------------------------------
//assert function

#define VeAssert(pred) {\
		if (!pred) {\
			std::cerr << "File " << __FILE__ << " line " << __LINE__ << ": Assertion failed!\n";\
			exit(1);\
		}\
	}



#endif

