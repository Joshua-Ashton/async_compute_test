#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <vulkan/vulkan_core.h>

#include <sys/capability.h>
#include <sys/prctl.h>
#include <sys/auxv.h>

#define MAX2(x, y) (((x) > (y)) ? (x) : (y))
#define MIN2(x, y) (((x) < (y)) ? (x) : (y))

static void print_caps()
{
	cap_t current_caps = cap_get_proc();
	printf("Has caps: %s - AT_SECURE: %lu\n", cap_to_text(current_caps, NULL), getauxval(AT_SECURE));
}

static bool check_nice()
{
    cap_t current_caps = cap_get_proc();
    cap_flag_value_t nice_cap_value = CAP_CLEAR;
    cap_get_flag(current_caps, CAP_SYS_NICE, CAP_EFFECTIVE, &nice_cap_value);

    return nice_cap_value == CAP_SET;
}

int main(int argc, char** argv)
{
    print_caps();

    bool has_nice_cap = check_nice();
    if (!has_nice_cap)
    {
        printf("Does not have CAP_SYS_NICE.\n");
        return 0;
    }

	const VkApplicationInfo application_info = {
		.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName   = "async_compute_test",
		.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		.pEngineName        = "no engine",
		.engineVersion      = VK_MAKE_VERSION(1, 0, 0),
		.apiVersion         = VK_API_VERSION_1_2,
	};

	const VkInstanceCreateInfo instance_create_info = {
		.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo        = &application_info,
	};

	VkInstance instance = VK_NULL_HANDLE;
	VkResult result = vkCreateInstance(&instance_create_info, 0, &instance);
	if (result != VK_SUCCESS)
	{
		fprintf(stderr, "vkCreateInstance failed: %d\n", result);
        return 1;
	}

	uint32_t physical_device_count = 128;
    VkPhysicalDevice physical_devices[128];
	result = vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices);
    if (result != VK_SUCCESS)
    {
		fprintf(stderr, "vkEnumeratePhysicalDevices failed: %d\n", result);
        return 1;
    }

    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    uint32_t queue_family = ~0u;
    for (uint32_t i = 0; i < physical_device_count; i++)
    {
        VkPhysicalDevice current_physical_device = physical_devices[i];

		VkPhysicalDeviceProperties device_properties;
		vkGetPhysicalDeviceProperties(current_physical_device, &device_properties);

		if (device_properties.apiVersion < VK_API_VERSION_1_2)
			continue;

		uint32_t queue_family_count = 128;
        VkQueueFamilyProperties queue_family_properties[128];
		vkGetPhysicalDeviceQueueFamilyProperties(current_physical_device, &queue_family_count, queue_family_properties);

        if (result != VK_SUCCESS)
        {
            fprintf(stderr, "vkGetPhysicalDeviceQueueFamilyProperties failed: %d\n", result);
            continue;
        }

		uint32_t general_queue_family = ~0u;
		uint32_t compute_queue_family = ~0u;
		for (uint32_t j = 0; j < queue_family_count; j++) {
			const VkQueueFlags general_bits = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT;

			if ((queue_family_properties[j].queueFlags & general_bits) == general_bits)
				general_queue_family = MIN2(general_queue_family, j);
			else if (queue_family_properties[j].queueFlags & VK_QUEUE_COMPUTE_BIT)
				compute_queue_family = MIN2(compute_queue_family, j);
		}

        if (compute_queue_family != ~0u)
        {
            queue_family = compute_queue_family;
            physical_device = current_physical_device;
            break;
        }
    }

    if (queue_family == ~0u)
    {
		fprintf(stderr, "Couldn't find physical device or queue family\n");
        return 1;
    }

	float queue_priority = 1.0f;

	VkDeviceQueueGlobalPriorityCreateInfoEXT queue_global_priority_info = {
		.sType          = VK_STRUCTURE_TYPE_DEVICE_QUEUE_GLOBAL_PRIORITY_CREATE_INFO_EXT,
		.pNext          = NULL,
		.globalPriority = VK_QUEUE_GLOBAL_PRIORITY_REALTIME_EXT
	};

	VkDeviceQueueCreateInfo queue_create_info = {
		.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
		.pNext            = &queue_global_priority_info,
		.queueFamilyIndex = queue_family,
		.queueCount       = 1,
		.pQueuePriorities = &queue_priority
	};

	VkDeviceCreateInfo device_create_info = {
		.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.queueCreateInfoCount = 1,
		.pQueueCreateInfos    = &queue_create_info,
	};

    VkDevice device = VK_NULL_HANDLE;
    result = vkCreateDevice(physical_device, &device_create_info, NULL, &device);

    if (result == VK_ERROR_NOT_PERMITTED_KHR)
    {
        printf("No permission to create device with realtime priority\n");
        return 0;
    }
    else if (result != VK_SUCCESS)
    {
		fprintf(stderr, "Couldn't create device: %d\n", result);
        return 1;
    }

    printf("Device created with realtime priority successfully!\n");

    return 0;
}