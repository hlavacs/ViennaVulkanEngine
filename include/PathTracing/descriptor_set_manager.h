#pragma once

namespace vve {
	class DescriptorInput {
	protected:
		VkDescriptorType type;
		size_t maxDescriptorCount = 1;
		size_t descriptorCount = 1;
		VkDescriptorBindingFlags bindingFlags;
		bool useVariableDescriptorCount = false;
	public:
		DescriptorInput(VkDescriptorType type, size_t descriptorCount)
			: type(type), descriptorCount(descriptorCount), maxDescriptorCount(1), bindingFlags(0), useVariableDescriptorCount(false){}

		DescriptorInput(VkDescriptorType type, size_t descriptorCount, size_t maxDescriptorCount, VkDescriptorBindingFlags bindingFlags, bool useVariableDescriptorCount = false)
			: type(type), descriptorCount(descriptorCount), maxDescriptorCount(maxDescriptorCount), bindingFlags(bindingFlags), useVariableDescriptorCount(useVariableDescriptorCount) {
			std::cout << "maxDescriptorCount: " << maxDescriptorCount << "\n";
		}

		virtual VkWriteDescriptorSet getDescriptorWrite() = 0;
		VkDescriptorSetLayoutBinding getDescriptorLayout() {
			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.descriptorType = type;
			layoutBinding.descriptorCount = maxDescriptorCount;
			layoutBinding.pImmutableSamplers = nullptr; // Optional

			return layoutBinding;
		}
		size_t getMaxDescriptorCount() { return maxDescriptorCount; }
		virtual size_t getDescriptorCount() { return descriptorCount; }
		VkDescriptorType getType() { return type; }
		VkDescriptorBindingFlags getBindingFlags() { return bindingFlags; }
		bool usesVariableDescriptorCount() {return useVariableDescriptorCount;}
	};

	class DescriptorBufferInput : public DescriptorInput {
	private:
		GenericBuffer* input;
		VkDescriptorBufferInfo bufferInfo;
	public:
		DescriptorBufferInput(GenericBuffer* input, VkDescriptorType type) :DescriptorInput(type, 1), input(input) {}

		GenericBuffer* getInput() {
			return input;
		}

		VkDescriptorType getType() {
			return type;
		}

		void updateInput(GenericBuffer* newInput) {
			input = newInput;
		}

		VkWriteDescriptorSet getDescriptorWrite() {
			bufferInfo = VkDescriptorBufferInfo();
			bufferInfo.buffer = input->getBuffer();
			bufferInfo.offset = 0;
			bufferInfo.range = input->getSize();

			VkWriteDescriptorSet descriptorWrite{};

			descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = type;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = &bufferInfo;

			return descriptorWrite;
		}
	};

	class DescriptorImageInput : public DescriptorInput {
	private:
		Image* input;
		VkDescriptorImageInfo imageInfo;
	public:
		DescriptorImageInput(Image* input, VkDescriptorType type) :DescriptorInput(type, 1), input(input){}

		Image* getInput() {
			return input;
		}

		VkDescriptorType getType() {
			return type;
		}

		void updateInput(Image* newInput) {
			input = newInput;
		}

		VkWriteDescriptorSet getDescriptorWrite() {
			imageInfo = VkDescriptorImageInfo();
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageInfo.imageView = input->getImageView();
			imageInfo.sampler = VK_NULL_HANDLE; // not used

			VkWriteDescriptorSet imageWrite{};

			imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			imageWrite.dstArrayElement = 0;
			imageWrite.descriptorType = type;
			imageWrite.descriptorCount = 1;
			imageWrite.pImageInfo = &imageInfo;

			return imageWrite;
		}
	};

	//TODO: std::vector<Texture*> is not a pointer textures will not update automatically!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	class DescriptorTextureInput : public DescriptorInput {
	private:
		std::vector<Texture*>* input;
		std::vector<VkDescriptorImageInfo> imageInfos;
		VkSampler sampler;
	public:
		DescriptorTextureInput(std::vector<Texture*>* input, VkSampler sampler, VkDescriptorType type, size_t maxTextureCount)
			:DescriptorInput(type, input->size(), maxTextureCount, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
				VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT |
				VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT, true), input(input), sampler(sampler) {
			std::cout << "maxTextureCount: " << maxTextureCount << "\n";
		}

		std::vector<Texture*>* getInput() {
			return input;
		}

		VkDescriptorType getType() {
			return type;
		}

		void updateInput(std::vector<Texture*>* newInput) {
			input = newInput;
		}

		size_t getDescriptorCount() override { return input->size(); }

		VkWriteDescriptorSet getDescriptorWrite() {
			size_t textureCount = input->size();
			imageInfos = std::vector<VkDescriptorImageInfo>(textureCount);
			for (uint32_t t = 0; t < textureCount; t++) {
				imageInfos[t].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageInfos[t].imageView = input->at(t)->image->getImageView();
				imageInfos[t].sampler = sampler;
			}

			VkWriteDescriptorSet imageWrite{};

			imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			imageWrite.dstArrayElement = 0;
			imageWrite.descriptorType = type;
			imageWrite.descriptorCount = textureCount;
			imageWrite.pImageInfo = imageInfos.data();

			return imageWrite;
		}
	};

	class DescriptorAccelInput : public DescriptorInput {
	private:
		//AccelStructure needs to be a pointer in order for it to automatically update
		AccelStructure* input;
		VkWriteDescriptorSetAccelerationStructureKHR asInfo;
		VkSampler sampler;
	public:
		DescriptorAccelInput(AccelStructure* input, VkDescriptorType type) :DescriptorInput(type, 1), input(input) {}

		AccelStructure* getInput() {
			return input;
		}

		VkDescriptorType getType() {
			return type;
		}

		void updateInput(AccelStructure* newInput) {
			input = newInput;
		}

		VkWriteDescriptorSet getDescriptorWrite() {

			asInfo = VkWriteDescriptorSetAccelerationStructureKHR{};
			asInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
			asInfo.accelerationStructureCount = 1;
			asInfo.pAccelerationStructures = &input->accel;

			VkWriteDescriptorSet tlasWrite{};
			tlasWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			tlasWrite.dstArrayElement = 0;
			tlasWrite.descriptorType = type;
			tlasWrite.descriptorCount = 1;
			tlasWrite.pNext = &asInfo;

			return tlasWrite;
		}
	};

	class DescriptorPlacment {
	protected:
		size_t dstBinding;
	public:
		DescriptorPlacment(size_t dstBinding) : dstBinding(dstBinding) {}
		virtual VkWriteDescriptorSet getDescriptorWrite(int frameInFlight) = 0;
		virtual DescriptorInput* getDescriptorInput(int frameInFlight) = 0;
		virtual VkDescriptorSetLayoutBinding getDescriptorLayout() = 0;
		size_t getDstbinding() const {
			return dstBinding;
		}
	};

	class SingleDescriptorPlacment : public DescriptorPlacment {
	private:
		DescriptorInput* input;
	public:
		SingleDescriptorPlacment(DescriptorInput* input, size_t dstBinding) : DescriptorPlacment(dstBinding), input(input) {}
		VkWriteDescriptorSet getDescriptorWrite(int frameInFlight) {
			VkWriteDescriptorSet set = input->getDescriptorWrite();
			set.dstBinding = dstBinding;
			return set;
		}

		DescriptorInput* getDescriptorInput(int frameInFlight) {
			return input;
		}

		VkDescriptorSetLayoutBinding getDescriptorLayout() {
			VkDescriptorSetLayoutBinding layoutBinding = input->getDescriptorLayout();
			layoutBinding.binding = dstBinding;
			return layoutBinding;
		}
	};

	class PerFrameDescriptorPlacment : public DescriptorPlacment {
	private:
		std::vector<DescriptorInput*> inputs;
	public:
		PerFrameDescriptorPlacment(std::vector<DescriptorInput*> inputs, size_t dstBinding) : DescriptorPlacment(dstBinding), inputs(inputs) {}
		VkWriteDescriptorSet getDescriptorWrite(int frameInFlight) {
			VkWriteDescriptorSet set = inputs[frameInFlight]->getDescriptorWrite();
			set.dstBinding = dstBinding;
			return set;
		}
		DescriptorInput* getDescriptorInput(int frameInFlight) {
			return inputs[frameInFlight];
		}
		VkDescriptorSetLayoutBinding getDescriptorLayout() {
			VkDescriptorSetLayoutBinding layoutBinding = inputs[0]->getDescriptorLayout();
			layoutBinding.binding = dstBinding;
			return layoutBinding;
		}
	};


	class DescriptorManager {
	private:
		bool finalized = false;
		bool discriptorsCreated = false;

		std::vector<DescriptorPlacment*> descriptorInputs;

		VkDescriptorSetLayout descriptorSetLayout;
		std::vector<VkDescriptorSet> descriptorSets;
		VkDescriptorPool descriptorPool;
		VkShaderStageFlags stageFlags;

		VkDevice& device;


		void createDescriptorPool() {
			std::unordered_map<VkDescriptorType, int> map;

			for (DescriptorPlacment* descriptorInput : descriptorInputs) {
				VkDescriptorType type = descriptorInput->getDescriptorInput(0)->getType();
				size_t maxCount = descriptorInput->getDescriptorInput(0)->getMaxDescriptorCount();

				if (map.contains(type)) {
					map[type] += maxCount;
				}
				else {
					map.insert({ type, maxCount });
				}
			}

			const size_t typeCount = map.size();

			std::vector<VkDescriptorPoolSize> poolSizes{ typeCount };

			int i = 0;
			for (std::pair<VkDescriptorType, int> descriptor : map) {
				poolSizes[i].type = descriptor.first;
				poolSizes[i].descriptorCount = descriptor.second * MAX_FRAMES_IN_FLIGHT;

				i++;
			}

			VkDescriptorPoolCreateInfo poolInfo{};
			poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
			poolInfo.pPoolSizes = poolSizes.data();
			poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
			poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

			if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor pool!");
			}
		}

		void createDescriptorSetLayout() {

			std::vector<VkDescriptorSetLayoutBinding> layoutBindings(descriptorInputs.size());

			for (int i = 0; i < descriptorInputs.size(); i++) {
				VkDescriptorSetLayoutBinding layoutbinding = descriptorInputs[i]->getDescriptorLayout();
				layoutbinding.stageFlags = stageFlags;

				std::cout << "Layout binding: " << layoutbinding.binding << "\n";
				std::cout << "Layout descriptor count: " << layoutbinding.descriptorCount << "\n";

				if (layoutbinding.binding != i) {
					std::cerr << "Encountered unexpected binding index";
				}

				layoutBindings[i] = layoutbinding;
			}

			std::vector<VkDescriptorBindingFlags> layoutBindingFlags(descriptorInputs.size());

			for (int i = 0; i < descriptorInputs.size(); i++) {
				VkDescriptorBindingFlags bindingFlags = descriptorInputs[i]->getDescriptorInput(0)->getBindingFlags();
				layoutBindingFlags[i] = bindingFlags;
			}


			VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
			bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			bindingFlagsInfo.bindingCount = static_cast<uint32_t>(layoutBindingFlags.size());
			bindingFlagsInfo.pBindingFlags = layoutBindingFlags.data();


			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());
			layoutInfo.pBindings = layoutBindings.data();
			layoutInfo.pNext = &bindingFlagsInfo;
			layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

			if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create descriptor set layout!");
			}

		}

		void createDescriptorSets() {

			discriptorsCreated = true;

			std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
			VkDescriptorSetAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			allocInfo.descriptorPool = descriptorPool;
			allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
			allocInfo.pSetLayouts = layouts.data();

			DescriptorInput* lastInput = descriptorInputs[descriptorInputs.size() - 1]->getDescriptorInput(0);
			VkDescriptorSetVariableDescriptorCountAllocateInfo countInfo{};
			std::vector<uint32_t> counts;

			if (lastInput->usesVariableDescriptorCount()) {
				countInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
				countInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
				counts = std::vector<uint32_t>(MAX_FRAMES_IN_FLIGHT, lastInput->getDescriptorCount());
				countInfo.pDescriptorCounts = counts.data();

				allocInfo.pNext = &countInfo;
			}
			
			descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
			if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
				throw std::runtime_error("failed to allocate descriptor sets!");
			}

			for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
				std::vector<VkWriteDescriptorSet> descriptorWrites(descriptorInputs.size());
				for (int b = 0; b < descriptorInputs.size(); b++) {
					VkWriteDescriptorSet descriptorWrite = descriptorInputs[b]->getDescriptorWrite(i);			
					descriptorWrite.dstSet = descriptorSets[i];
					descriptorWrites[b] = descriptorWrite;
				}
				vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
			}
		}

	public:
		DescriptorManager(std::vector<DescriptorPlacment*> descriptorInputs, VkShaderStageFlags stageFlags, VkDevice& device) : descriptorInputs(descriptorInputs), stageFlags(stageFlags), device(device) {}
		DescriptorManager(VkShaderStageFlags stageFlags, VkDevice& device) : descriptorInputs(std::vector<DescriptorPlacment*>()), stageFlags(stageFlags), device(device) {}

		void finalize() {
			finalized = true;
			std::sort(descriptorInputs.begin(), descriptorInputs.end(), [](const DescriptorPlacment* a, const DescriptorPlacment* b) {
				return a->getDstbinding() < b->getDstbinding();
				});
			createDescriptorPool();
			createDescriptorSetLayout();
			//at the point at which the descriptor sets are created the buffers are empty leading to a error. Pospone descriptor set creation to a later point.
			//createDescriptorSets();
		}

		void update() {
			if (!finalized) {
				std::cerr << "Descriptor Manager has to be finalized before updating the descriptor sets \n";
			}

			if (discriptorsCreated) {
				vkDeviceWaitIdle(device);
				vkFreeDescriptorSets(
					device,
					descriptorPool,
					descriptorSets.size(),
					descriptorSets.data()
				);
			}

			createDescriptorSets();
		}

		void addDescriptorInput(DescriptorPlacment* input) {
			if (finalized) {
				std::cerr << "descriptor inputs may not be added after finalizing Descriptor Manager \n";
			}
			descriptorInputs.push_back(input);
		}

		std::vector<VkDescriptorSet> getDescriptorSets() {
			if (!finalized) {
				std::cerr << "Descriptor Manager has to be finalized before retrieve \n";
			}
			return descriptorSets;
		}
	};
}