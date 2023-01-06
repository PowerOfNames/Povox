#pragma once

#include <vulkan/vulkan.h>
#define PX_VK_VALIDATION_ALLOWED true
#ifdef PX_DEBUG
	#if PX_VK_VALIDATION_ALLOWED
		#define PX_ENABLE_VK_VALIDATION_LAYERS true
	#else
		#define PX_ENABLE_VK_VALIDATION_LAYERS false
	#endif //PX_VK_VALIDATION_ALLOWED
	#ifdef PX_ENABLE_ASSERT
		#define PX_ENABLE_VK_ASSERT
	#endif //PX_ENABLE_ASSERT
#endif //PX_DEBUG

	


#ifdef PX_ENABLE_VK_ASSERT
	#define	PX_CORE_VK_ASSERT(x, y, ...) {if(!(x == y)) {PX_CORE_ERROR("Assertion fails: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define	PX_CORE_VK_ASSERT(x, y, ...) { x; }
#endif


namespace Povox {

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
		void* userData)
	{
		switch (messageSeverity)
		{
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		{
			PX_CORE_INFO("VK-Debug Callback: '{0}'", callbackData->pMessage);
			break;
		}
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		{
			PX_CORE_WARN("VK-Debug Callback: '{0}'", callbackData->pMessage);
			break;
		}
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		{
			PX_CORE_ERROR("VK-Debug Callback: '{0}'", callbackData->pMessage);
			break;
		}
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		{
			PX_CORE_TRACE("VK-Debug Callback: '{0}'", callbackData->pMessage);
			break;
		}
		}
		return VK_FALSE;
	}

	static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* createInfo, const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debugMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
			return func(instance, createInfo, allocator, debugMessenger);
		else
			return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

	static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* allocator) {
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr) {
			func(instance, debugMessenger, allocator);
		}
	}

	static VkResult NameVkObject(VkDevice device, VkDebugUtilsObjectNameInfoEXT objNameInfo)
	{
		auto func = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT");
		if (func != nullptr)
			return func(device, &objNameInfo);
		return VK_INCOMPLETE;
	}
}
