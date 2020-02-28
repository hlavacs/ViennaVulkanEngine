#pragma once



namespace vve::dev {


	VkResult vhCreateInstance(std::vector<const char*>& extensions, std::vector<const char*>& validationLayers, VkInstance* instance);


}

