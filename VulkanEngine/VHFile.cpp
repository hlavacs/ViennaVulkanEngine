/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/


#include "VHHelper.h"

namespace vh {

	//-------------------------------------------------------------------------------------------------------
	/**
	*
	* \brief Read the contents of a file and return a binary blob with it
	*
	* \param[in] filename Filename
	* \returns a binary blob containing the file.
	*
	*/

	std::vector<char> vhFileRead(const std::string& filename) {
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open()) {
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}
	

}



