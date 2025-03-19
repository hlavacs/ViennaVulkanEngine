/**
* The Vienna Vulkan Engine
*
* (c) bei Helmut Hlavacs, University of Vienna
*
*/

#include "VHHelper.h"

namespace vh
{
	//-------------------------------------------------------------------------------------------------------
	/**
		*
		* \brief Read the contents of a file and return a binary blob with it
		*
		* \param[in] filename Filename
		* \returns a binary blob containing the file.
		*
		*/

	std::vector<char> vhFileRead(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			assert(false);
			exit(1);
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	/**
		*
		* \brief Take a time measurment from the high resolution clock
		* \returns the measured value of the clock
		*
		*/
	std::chrono::high_resolution_clock::time_point vhTimeNow()
	{
		return std::chrono::high_resolution_clock::now();
	}

	/**
		*
		* \brief Use the high resolution clock to calculate a time duration since the last time measurement
		*
		* \param[in] t_prev The last time measruement
		* \returns the measured time duration
		*
		*/
	float vhTimeDuration(std::chrono::high_resolution_clock::time_point t_prev)
	{
		std::chrono::duration<double> time_span =
			std::chrono::duration_cast<std::chrono::duration<double>>(vhTimeNow() - t_prev);
		return std::chrono::duration_cast<std::chrono::microseconds>(time_span).count() / 1.0e6f;
	}

	/**
		*
		* \brief Apply expoentatial smoothing to given values
		*
		* \param[in] new_val The newest value that was measured
		* \param[in] average The average so far
		* \param[in] weight An averaging weight between 0 and 1
		* \returns The new average
		*
		*/
	float vhAverage(float new_val, float average, float weight)
	{
		return weight * average + (1.0f - weight) * new_val;
	}

} // namespace vh
