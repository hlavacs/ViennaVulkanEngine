/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#ifndef VENAMEDCLASS_H
#define VENAMEDCLASS_H

namespace ve
{
	/**
		*
		* \brief Base class of all classes that need a name.
		*
		* Names do not have to be unique, only if they are used to identify things in a collection.
		*
		*/
	class VENamedClass
	{
	protected:
		std::string m_name; ///<Name of this instance

	public:
		///Constructor
		VENamedClass(std::string name)
		{
			m_name = name;
		};

		///Destructor
		~VENamedClass() {};

		std::string getName(); //get the name
	};

} // namespace ve

#endif
