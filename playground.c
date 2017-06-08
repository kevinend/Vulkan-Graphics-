#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

// Load from the platform -- windows
PFN_vkGetInstanceProcAddr						vkGetInstanceProcAddr;

// Load at global level (no instance)
PFN_vkCreateInstance							vkCreateInstance;
PFN_vkEnumerateInstanceExtensionProperties		vkEnumerateInstanceExtensionProperties;
PFN_vkEnumerateInstanceLayerProperties			vkEnumerateInstanceLayerProperties;

// Load at instance level
PFN_vkEnumeratePhysicalDevices  				vkEnumeratePhysicalDevices;
PFN_vkEnumerateDeviceExtensionProperties		vkEnumerateDeviceExtensionProperties;
PFN_vkGetPhysicalDeviceProperties				vkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceFeatures					vkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceQueueFamilyProperties    vkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkCreateDevice								vkCreateDevice;
PFN_vkDestroyDevice								vkDestroyDevice;
PFN_vkGetDeviceProcAddr							vkGetDeviceProcAddr;
PFN_vkDestroyInstance							vkDestroyInstance;

// Load at instance level -- extensions
PFN_vkCreateWin32SurfaceKHR						vkCreateWin32SurfaceKHR;
PFN_vkDestroySurfaceKHR							vkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR		vkGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR		vkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	vkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR   vkGetPhysicalDeviceSurfaceCapabilitiesKHR;

// Load at device level 
PFN_vkGetDeviceQueue							vkGetDeviceQueue;
PFN_vkCreateSemaphore							vkCreateSemaphore;
PFN_vkDestroySemaphore							vkDestroySemaphore;
PFN_vkDestroyDevice								vkDestroyDevice;
PFN_vkDeviceWaitIdle							vkDeviceWaitIdle;

PFN_vkCreateCommandPool							vkCreateCommandPool;
PFN_vkAllocateCommandBuffers					vkAllocateCommandBuffers;
PFN_vkQueueSubmit								vkQueueSubmit;
PFN_vkBeginCommandBuffer						vkBeginCommandBuffer;
PFN_vkCmdPipelineBarrier						vkCmdPipelineBarrier;
PFN_vkCmdClearColorImage						vkCmdClearColorImage;
PFN_vkEndCommandBuffer							vkEndCommandBuffer;

// Load at device level -- extensions 
PFN_vkCreateSwapchainKHR   						vkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR   					vkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR 					vkGetSwapchainImagesKHR;
PFN_vkAcquireNextImageKHR  					    vkAcquireNextImageKHR;
PFN_vkQueuePresentKHR							vkQueuePresentKHR;

// Globals
FILE *windows_error_log_file;

char *required_instance_extensions[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
};

uint32_t count_of_required_instance_extensions = (sizeof required_instance_extensions) / (sizeof required_instance_extensions[0]);

char *required_device_extensions[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

uint32_t count_of_required_device_extensions = (sizeof required_device_extensions) / (sizeof required_device_extensions[0]);

bool window_open = true;

typedef struct {

	VkInstance 			instance;
	VkPhysicalDevice	physical_device;
	VkDevice			logical_device;
	VkSurfaceKHR 		surface;
	uint32_t 			queue_family_index;
	VkQueue				graphics_queue;
	VkQueue				present_queue;
	VkSwapchainKHR		swap_chain;
	uint32_t			count_of_swap_chain_images;
	VkSemaphore			image_available;
	VkSemaphore			rendering_complete;
	VkCommandPool		command_pool;
	VkCommandBuffer     *command_buffers;
	uint32_t			count_of_command_buffers;

} Vulkan_Context;

Vulkan_Context vulkan_context = { 0 };

void
get_last_error_as_string( char *function_name )
{
	DWORD error_code = GetLastError();
	if ( error_code == 0 ) {
		return;
	}

	char *message_buffer;
	FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
				   0, error_code, MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ),
				   (LPTSTR)&message_buffer, 0, 0 );

	fprintf( windows_error_log_file, function_name );
	fprintf( windows_error_log_file, message_buffer );
	LocalFree( message_buffer );

	return;
}

HMODULE
load_vulkan_library( void ) 
{
	HMODULE vulkan_library_handle;
	vulkan_library_handle = LoadLibrary( "vulkan-1.dll" );
	if ( !vulkan_library_handle ) {
		fprintf( stdout, "Unable to load the vulkan library\n" );
		exit( EXIT_FAILURE );
	}
	
	return vulkan_library_handle;
}


void 
load_vulkan_global_functions( HMODULE vulkan_library_handle )
{
	// Load from platform level (windows - GetProcAddress)
	vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) GetProcAddress( vulkan_library_handle, "vkGetInstanceProcAddr" );

	// Load from global level (no instance required)
	vkCreateInstance                       = (PFN_vkCreateInstance) 					  vkGetInstanceProcAddr( NULL, "vkCreateInstance" );
	vkEnumerateInstanceExtensionProperties = (PFN_vkEnumerateInstanceExtensionProperties) vkGetInstanceProcAddr( NULL, "vkEnumerateInstanceExtensionProperties" );
	vkEnumerateInstanceLayerProperties     = (PFN_vkEnumerateInstanceLayerProperties)     vkGetInstanceProcAddr( NULL, "vkEnumerateInstanceLayerProperties" );

	return;
}


void
verify_instance_supports_required_extensions( Vulkan_Context *vulkan_context ) 
{
	VkResult result;
	uint32_t count_of_available_instance_extensions;

	result = vkEnumerateInstanceExtensionProperties( NULL, &count_of_available_instance_extensions, NULL );
	if ( result != VK_SUCCESS || count_of_available_instance_extensions == 0 ) {
		fprintf( stdout, "Unable to enumerate instance extensions\n" );
		exit( EXIT_FAILURE );
	}

	VkExtensionProperties *available_instance_extensions;
	available_instance_extensions = (VkExtensionProperties *)malloc( count_of_available_instance_extensions * sizeof (VkExtensionProperties) );
	if ( !available_instance_extensions ) {
		fprintf( stdout, "Unable to allocate space for available instance extensions\n" );
		exit( EXIT_FAILURE );
	}

	result = vkEnumerateInstanceExtensionProperties( NULL, &count_of_available_instance_extensions, available_instance_extensions );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Enumeration of extensions failed\n" );
		exit( EXIT_FAILURE );
	}


	bool *all_instance_extensions_found;
	all_instance_extensions_found = (bool *)calloc( count_of_required_instance_extensions, sizeof (bool) );
	if ( !all_instance_extensions_found ) {
		fprintf( stdout, "Unable to allocate boolean array to test supported extensions\n" );
		exit( EXIT_FAILURE );
	}

	for ( uint32_t i = 0; i < count_of_required_instance_extensions; ++i ) {
		for ( uint32_t j = 0; j < count_of_available_instance_extensions; ++j ) {
			if ( strcmp( required_instance_extensions[i], available_instance_extensions[j].extensionName ) == 0 ) {
				all_instance_extensions_found[i] = true;
				break;
			}
		}
	}

	for ( uint32_t i = 0; i < count_of_required_instance_extensions; ++i ) {
		if ( all_instance_extensions_found[i] == false ) {
			fprintf( stdout, "Required instance extension not found on the system\n" );
			exit( EXIT_FAILURE );
		}
	}	

	free( available_instance_extensions );
	free( all_instance_extensions_found );

	return;
}


// Could split this out into three separate functions -- chose not to at the moment
VkInstance 
create_vulkan_instance( Vulkan_Context *vulkan_context )
{
	VkApplicationInfo application_info = { 0 };

	application_info.sType 				= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application_info.pApplicationName   = "Hello Triangle";
	application_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0 );
	application_info.pEngineName 		= "No Engine";
	application_info.engineVersion 		= VK_MAKE_VERSION(1, 0, 0);
	application_info.apiVersion 		= VK_API_VERSION_1_0;


	VkInstanceCreateInfo instance_create_info = { 0 };

	instance_create_info.sType 					 = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.pApplicationInfo        = &application_info;
	instance_create_info.enabledExtensionCount   = count_of_required_instance_extensions;
	instance_create_info.ppEnabledExtensionNames = required_instance_extensions;	


	VkInstance vulkan_instance;
	VkResult result;

	result = vkCreateInstance( &instance_create_info, NULL, &vulkan_instance );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to create a new Vulkan instance\n" );
		exit( EXIT_FAILURE );
	}

	return vulkan_instance;
}


void
load_vulkan_instance_functions( Vulkan_Context *vulkan_context ) 
{
	vkEnumeratePhysicalDevices     			   = (PFN_vkEnumeratePhysicalDevices)                vkGetInstanceProcAddr( vulkan_context->instance, "vkEnumeratePhysicalDevices" );
	vkEnumerateDeviceExtensionProperties       = (PFN_vkEnumerateDeviceExtensionProperties)      vkGetInstanceProcAddr( vulkan_context->instance, "vkEnumerateDeviceExtensionProperties" );
	vkGetPhysicalDeviceProperties  			   = (PFN_vkGetPhysicalDeviceProperties)             vkGetInstanceProcAddr( vulkan_context->instance, "vkGetPhysicalDeviceProperties" );
	vkGetPhysicalDeviceFeatures    			   = (PFN_vkGetPhysicalDeviceFeatures)               vkGetInstanceProcAddr( vulkan_context->instance, "vkGetPhysicalDeviceFeatures" );
	vkGetPhysicalDeviceQueueFamilyProperties   = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)  vkGetInstanceProcAddr( vulkan_context->instance, "vkGetPhysicalDeviceQueueFamilyProperties" );
	vkCreateDevice                 			   = (PFN_vkCreateDevice)                            vkGetInstanceProcAddr( vulkan_context->instance, "vkCreateDevice" );
	vkDeviceWaitIdle						   = (PFN_vkDeviceWaitIdle)						     vkGetInstanceProcAddr( vulkan_context->instance, "vkDeviceWaitIdle" );
	vkDestroyDevice							   = (PFN_vkDestroyDevice)						     vkGetInstanceProcAddr( vulkan_context->instance, "vkDestroyDevice" );
	vkGetDeviceProcAddr            			   = (PFN_vkGetDeviceProcAddr)                       vkGetInstanceProcAddr( vulkan_context->instance, "vkGetDeviceProcAddr" );
	vkDestroyInstance              			   = (PFN_vkDestroyInstance)                         vkGetInstanceProcAddr( vulkan_context->instance, "vkDestroyInstance" );

	return;	
}


void  
load_vulkan_instance_extension_functions( Vulkan_Context *vulkan_context )
{
	vkCreateWin32SurfaceKHR                    = (PFN_vkCreateWin32SurfaceKHR) 				     vkGetInstanceProcAddr( vulkan_context->instance, "vkCreateWin32SurfaceKHR" );
	vkDestroySurfaceKHR                        = (PFN_vkDestroySurfaceKHR)				   	     vkGetInstanceProcAddr( vulkan_context->instance, "vkDestroySurfaceKHR" );
	vkGetPhysicalDeviceSurfaceSupportKHR       = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)	     vkGetInstanceProcAddr( vulkan_context->instance, "vkGetPhysicalDeviceSurfaceSupportKHR" );
	vkGetPhysicalDeviceSurfaceFormatsKHR       = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)	     vkGetInstanceProcAddr( vulkan_context->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR" );
	vkGetPhysicalDeviceSurfacePresentModesKHR  = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR) vkGetInstanceProcAddr( vulkan_context->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR" );
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR  = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR) vkGetInstanceProcAddr( vulkan_context->instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR" );

	return;	
}

VkSurfaceKHR
create_vulkan_surface( Vulkan_Context *vulkan_context, HINSTANCE windows_instance, HWND window_handle ) 
{
	VkWin32SurfaceCreateInfoKHR surface_create_info = { 0 };
	
	surface_create_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_create_info.hinstance = windows_instance;
	surface_create_info.hwnd      = window_handle;

	VkSurfaceKHR surface;
	VkResult result;

	result = vkCreateWin32SurfaceKHR( vulkan_context->instance, &surface_create_info, NULL, &surface );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to create a Win32 surface for rendering\n" );
		exit( EXIT_FAILURE );
	}

	return surface;
}


// NOTE: in/out param -- physical_device_count
VkPhysicalDevice * 
find_vulkan_enabled_physical_devices( Vulkan_Context *vulkan_context, uint32_t *physical_device_count ) 
{
	VkResult result;
	result = vkEnumeratePhysicalDevices( vulkan_context->instance, physical_device_count, NULL );
	if ( result != VK_SUCCESS || physical_device_count == 0 ) {
		fprintf( stdout, "Unable to find any available devices\n" );
		exit( EXIT_FAILURE );
	}

	VkPhysicalDevice *physical_devices;
	physical_devices = (VkPhysicalDevice *)malloc( *physical_device_count * sizeof (VkPhysicalDevice) );
	if ( !physical_devices ) {
		fprintf( stdout, "Unable to allocate memory for physical devices\n" );
		exit( EXIT_FAILURE );
	}

	result = vkEnumeratePhysicalDevices( vulkan_context->instance, physical_device_count, physical_devices );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to acquire handles to physical devices\n" );
		exit( EXIT_FAILURE );
	}

	return physical_devices;
}

VkPhysicalDevice 
find_discrete_graphics_card( VkPhysicalDevice *physical_devices, uint32_t physical_device_count )
{
	VkPhysicalDevice			current_device;
	VkPhysicalDeviceProperties	current_device_properties;
	VkPhysicalDevice 			selected_device;

	selected_device = VK_NULL_HANDLE;
	for ( uint32_t i = 0; i < physical_device_count; ++i ) {
		current_device = physical_devices[i];
		vkGetPhysicalDeviceProperties( current_device, &current_device_properties );

		if ( current_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			selected_device = current_device;
			break;
		}
	}

	if ( selected_device == VK_NULL_HANDLE ) {
		fprintf( stdout, "Unable to find a dedicated graphics card on the machine\n" );
		exit( EXIT_FAILURE );
	}

	return selected_device;
}	

void 
verify_physical_device_supports_required_extensions( VkPhysicalDevice selected_device )
{
	uint32_t count_of_available_device_extensions;
	vkEnumerateDeviceExtensionProperties( selected_device, NULL, &count_of_available_device_extensions, NULL );
			
	if ( count_of_available_device_extensions == 0 ) {
		fprintf( stdout, "No device extensions available\n" );
		exit( EXIT_FAILURE );
	}

	VkExtensionProperties *available_device_extensions;
	available_device_extensions = (VkExtensionProperties *)malloc( count_of_available_device_extensions * sizeof (VkExtensionProperties) );
	if ( !available_device_extensions ) {
		fprintf( stdout, "Unable to allocate space for the available device extensions\n" );
		exit( EXIT_FAILURE );
	}

	vkEnumerateDeviceExtensionProperties( selected_device, NULL, &count_of_available_device_extensions, available_device_extensions );

	bool *all_device_extensions_found;
	all_device_extensions_found = (bool *)calloc( count_of_required_device_extensions, sizeof (bool) );
	if ( !all_device_extensions_found ) {
		fprintf( stdout, "Unable to allocate space to test whether all device extensions have been found\n" );
		exit( EXIT_FAILURE );
	}


	for ( uint32_t i = 0; i < count_of_required_device_extensions; ++i ) {
		for ( uint32_t j = 0; j < count_of_available_device_extensions; ++j ) {
			if ( strcmp( required_device_extensions[i], available_device_extensions[j].extensionName ) == 0 ) {
				all_device_extensions_found[i] = true;
				break;
			}
		}
		
	}

	for ( uint32_t i = 0; i < count_of_required_device_extensions; ++i ) {
		if ( all_device_extensions_found[i] == false ) {
			fprintf( stdout, "Required device extension not found on the system\n" );
			exit( EXIT_FAILURE );
		}
	}

	free( available_device_extensions );
	free( all_device_extensions_found );

	return;
}

uint32_t 
find_queue_family_with_queues_supporting_graphics_and_presentation( Vulkan_Context *vulkan_context )
{
	uint32_t count_of_queue_families;
	vkGetPhysicalDeviceQueueFamilyProperties( vulkan_context->physical_device, &count_of_queue_families, NULL );

	if ( count_of_queue_families == 0 ) { 
		fprintf( stdout, "Zero queue families found for the device\n" );
		exit( EXIT_FAILURE );
	}		

	VkQueueFamilyProperties *queue_family_properties;
	queue_family_properties = (VkQueueFamilyProperties *)malloc( count_of_queue_families * sizeof (VkQueueFamilyProperties) );
	if ( !queue_family_properties ) {
		fprintf( stdout, "Unable to allocate space for queue family properties\n" );
		exit( EXIT_FAILURE );
	}

	vkGetPhysicalDeviceQueueFamilyProperties( vulkan_context->physical_device, &count_of_queue_families, queue_family_properties );

	int suitable_queue_family_index			   = -1;	
	int queue_family_with_presentation_support = -2;
	int queue_family_with_graphics_support 	   = -3;

	for ( uint32_t i = 0; i < count_of_queue_families; ++i ) {

		if ( queue_family_properties[i].queueCount < 1 ) {
			continue;
		}
	
		// NOTE: checks whether a queue family of the device supports presentation to the given surface 
		VkBool32 has_presentation_support;
		vkGetPhysicalDeviceSurfaceSupportKHR( vulkan_context->physical_device, i, vulkan_context->surface, &has_presentation_support );
		if ( has_presentation_support == VK_TRUE ) {
			queue_family_with_presentation_support = i;
		}
		
		if ( queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) {
			queue_family_with_graphics_support = i;
		}

		if ( queue_family_with_graphics_support == queue_family_with_presentation_support ) {
			suitable_queue_family_index = queue_family_with_graphics_support;
		}
	}

	if ( suitable_queue_family_index == -1 ) {
		fprintf( stdout, "Unable to find a queue family that supports both graphics and presentation to selected surface\n" );
		exit( EXIT_FAILURE );
	}

	free( queue_family_properties );
	return suitable_queue_family_index;
}

VkDevice 
create_vulkan_logical_device( Vulkan_Context *vulkan_context ) 
{
	VkDeviceQueueCreateInfo queue_create_info = { 0 };
	float queue_priority = 1.0f;

	queue_create_info.sType 		   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_create_info.queueFamilyIndex = vulkan_context->queue_family_index;
	queue_create_info.queueCount       = 1;
	queue_create_info.pQueuePriorities = &queue_priority;

	VkDeviceCreateInfo device_create_info = { 0 };

	device_create_info.sType 				   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.queueCreateInfoCount    = 1;
	device_create_info.pQueueCreateInfos 	   = &queue_create_info;
	device_create_info.ppEnabledExtensionNames = required_device_extensions;

	VkResult result;
	VkDevice logical_device;

	logical_device = VK_NULL_HANDLE;
	result = vkCreateDevice( vulkan_context->physical_device, &device_create_info, NULL, &logical_device );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to create a logical device from the physical device\n" );
		exit( EXIT_FAILURE );
	}

	return logical_device;	
}

void 
load_vulkan_device_functions( Vulkan_Context *vulkan_context ) 
{
	vkGetDeviceQueue   = (PFN_vkGetDeviceQueue)   vkGetDeviceProcAddr( vulkan_context->logical_device, "vkGetDeviceQueue" );
	vkCreateSemaphore  = (PFN_vkCreateSemaphore)  vkGetDeviceProcAddr( vulkan_context->logical_device, "vkCreateSemaphore" );
	vkDestroySemaphore = (PFN_vkDestroySemaphore) vkGetDeviceProcAddr( vulkan_context->logical_device, "vkDestroySemaphore" );
	vkDestroyDevice    = (PFN_vkDestroyDevice)    vkGetDeviceProcAddr( vulkan_context->logical_device, "vkDestroyDevice" );
	vkDeviceWaitIdle   = (PFN_vkDeviceWaitIdle)   vkGetDeviceProcAddr( vulkan_context->logical_device, "vkDeviceWaitIdle" );	

	vkCreateCommandPool 	 = (PFN_vkCreateCommandPool)	  vkGetDeviceProcAddr( vulkan_context->logical_device, "vkCreateCommandPool" );
	vkAllocateCommandBuffers = (PFN_vkAllocateCommandBuffers) vkGetDeviceProcAddr( vulkan_context->logical_device, "vkAllocateCommandBuffers" );
	vkQueueSubmit            = (PFN_vkQueueSubmit)            vkGetDeviceProcAddr( vulkan_context->logical_device, "vkQueueSubmit" );
	vkBeginCommandBuffer     = (PFN_vkBeginCommandBuffer)     vkGetDeviceProcAddr( vulkan_context->logical_device, "vkBeginCommandBuffer" );
	vkCmdPipelineBarrier     = (PFN_vkCmdPipelineBarrier)     vkGetDeviceProcAddr( vulkan_context->logical_device, "vkCmdPipelineBarrier" );
	vkCmdClearColorImage     = (PFN_vkCmdClearColorImage)     vkGetDeviceProcAddr( vulkan_context->logical_device, "vkCmdClearColorImage" );
	vkEndCommandBuffer       = (PFN_vkEndCommandBuffer)		  vkGetDeviceProcAddr( vulkan_context->logical_device, "vkEndCommandBuffer" );

	return;	
}

void
load_vulkan_device_extension_functions( Vulkan_Context *vulkan_context ) 
{
	vkCreateSwapchainKHR    = (PFN_vkCreateSwapchainKHR)    vkGetDeviceProcAddr( vulkan_context->logical_device, "vkCreateSwapchainKHR" );
	vkDestroySwapchainKHR   = (PFN_vkDestroySwapchainKHR)   vkGetDeviceProcAddr( vulkan_context->logical_device, "vkDestroySwapchainKHR" );
	vkGetSwapchainImagesKHR = (PFN_vkGetSwapchainImagesKHR) vkGetDeviceProcAddr( vulkan_context->logical_device, "vkGetSwapchainImagesKHR" );
	vkAcquireNextImageKHR   = (PFN_vkAcquireNextImageKHR)   vkGetDeviceProcAddr( vulkan_context->logical_device, "vkAcquireNextImageKHR" );
	vkQueuePresentKHR       = (PFN_vkQueuePresentKHR)       vkGetDeviceProcAddr( vulkan_context->logical_device, "vkQueuePresentKHR" );

	return;
}

VkSurfaceCapabilitiesKHR
acquire_surface_and_swap_chain_capabilities( Vulkan_Context *vulkan_context ) 
{
	VkResult result;
	VkSurfaceCapabilitiesKHR surface_capabilities;
	result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR( vulkan_context->physical_device, vulkan_context->surface, &surface_capabilities );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to get presentation surface capabilities\n" );
		exit( EXIT_FAILURE );
	}

	return surface_capabilities;
}

VkSurfaceFormatKHR *
acquire_supported_surface_formats( Vulkan_Context *vulkan_context, uint32_t *count_of_surface_formats )
{
	VkResult result;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR( vulkan_context->physical_device, vulkan_context->surface, count_of_surface_formats, NULL );
	if ( result != VK_SUCCESS || *count_of_surface_formats == 0 ) {
		fprintf( stdout, "Unable to query surface formats\n" );
		exit( EXIT_FAILURE );
	}

	VkSurfaceFormatKHR *surface_formats;
	surface_formats = (VkSurfaceFormatKHR *)malloc( *count_of_surface_formats * sizeof (VkSurfaceFormatKHR) );
	if ( !surface_formats ) {
		fprintf( stdout, "Unable to allocate memory to store surface formats\n" );
		exit( EXIT_FAILURE );
	}

	vkGetPhysicalDeviceSurfaceFormatsKHR( vulkan_context->physical_device, vulkan_context->surface, count_of_surface_formats, surface_formats );
	
	return surface_formats;
}

VkPresentModeKHR *
acquire_supported_present_modes( Vulkan_Context *vulkan_context, uint32_t *count_of_present_modes ) 
{
	VkResult result;
	result = vkGetPhysicalDeviceSurfacePresentModesKHR( vulkan_context->physical_device, vulkan_context->surface, count_of_present_modes, NULL );
	if ( result != VK_SUCCESS || *count_of_present_modes == 0 ) {
		fprintf( stdout, "Unable to query present mode for surface\n" );
		exit( EXIT_FAILURE );
	}

	VkPresentModeKHR *present_modes;
	present_modes = (VkPresentModeKHR *)malloc( *count_of_present_modes * sizeof (VkPresentModeKHR) );
	if ( !present_modes ) {
		fprintf( stdout, "Unable to allocate memory to store surface present modes\n" );
		exit( EXIT_FAILURE );
	}
		
	vkGetPhysicalDeviceSurfacePresentModesKHR( vulkan_context->physical_device, vulkan_context->surface, count_of_present_modes, present_modes );

	return present_modes;
}

uint32_t
select_number_of_swap_chain_images( VkSurfaceCapabilitiesKHR *surface_capabilities )
{
	// want double buffering -- 2 write buffers while 1 being presented
	if ( surface_capabilities->maxImageCount < 3 ) {
		fprintf( stdout, "surface does not support double buffering\n" );
		exit( EXIT_FAILURE );
	}

	return surface_capabilities->maxImageCount; 
}

VkSurfaceFormatKHR
select_format_for_swap_chain_images( VkSurfaceFormatKHR *surface_formats, uint32_t count_of_surface_formats ) 
{
	uint32_t desired_format_index;
	desired_format_index = -1;
	for ( uint32_t i = 0; i < count_of_surface_formats; ++i ) {
		if ( surface_formats[i].format == VK_FORMAT_B8G8R8A8_UNORM
				&& surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ) {
			
			desired_format_index = i;
			break;
		}
	}

	if ( desired_format_index == -1 ) {
		fprintf( stdout, "Surface failed to meet desired format specs\n" );
		exit( EXIT_FAILURE );
	}
	
		return surface_formats[desired_format_index];
}

VkExtent2D
select_swap_chain_image_size( VkSurfaceCapabilitiesKHR *surface_capabilities ) 
{
	// width and height are initially chosen as defaults 
	VkExtent2D swap_chain_extent;
	swap_chain_extent.width  = 640;
	swap_chain_extent.height = 480;

	// -1 is a special value that means that the window size will be determined by the swap chain size
	// however, you must choose a size that is within the surfaces capabilities 
	if ( surface_capabilities->currentExtent.width == -1 ) {
	
		if ( swap_chain_extent.width < surface_capabilities->minImageExtent.width ) {
			swap_chain_extent.width = surface_capabilities->minImageExtent.width;
		}

		if ( swap_chain_extent.height < surface_capabilities->minImageExtent.height ) {
			swap_chain_extent.height = surface_capabilities->minImageExtent.height;	
		}

		if ( swap_chain_extent.width > surface_capabilities->maxImageExtent.width ) {
			swap_chain_extent.width = surface_capabilities->maxImageExtent.width;
		}

		if ( swap_chain_extent.height > surface_capabilities->maxImageExtent.height ) {
			swap_chain_extent.height = surface_capabilities->maxImageExtent.height;
		}
	}
	else {
		swap_chain_extent = surface_capabilities->currentExtent;
	}	

	return swap_chain_extent;
}

VkImageUsageFlags
select_swap_chain_usage_flags( VkSurfaceCapabilitiesKHR *surface_capabilities  ) 
{
	// Color attachment bit is always supported
	// NEED transfer destination usage which is required for image clear operation
	VkImageUsageFlags available_image_usage_flags;
	VkImageUsageFlags desired_flags[] = {
	VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	VK_IMAGE_USAGE_TRANSFER_DST_BIT,
	VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
	VK_IMAGE_USAGE_SAMPLED_BIT,
	VK_IMAGE_USAGE_STORAGE_BIT,
	VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
	VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
	};

	uint32_t count_of_desired_flags;
	count_of_desired_flags = (sizeof desired_flags) / (sizeof desired_flags[0]);

	for ( uint32_t i = 0; i < count_of_desired_flags; ++i ) {
		if ( surface_capabilities->supportedUsageFlags & desired_flags[i] ) {
			available_image_usage_flags |= desired_flags[i];
		}
	}

	return available_image_usage_flags;
}

VkSurfaceTransformFlagBitsKHR 
select_swap_chain_pre_transforms( VkSurfaceCapabilitiesKHR *surface_capabilities ) 
{
	// we don't want any transform to account for orientation (like a tablet)
	if ( surface_capabilities->supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ) {
		return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else {
		return surface_capabilities->currentTransform;
	}	
}

VkPresentModeKHR 
select_swap_chain_present_mode( VkPresentModeKHR *present_modes, uint32_t count_of_present_modes ) 
{
	// FIFO present mode is always available
	// MAILBOX is the lowest latency V-Sync enable mode (similar to triple buffering)
	uint32_t chosen_present_mode_index;
	for ( uint32_t i = 0; i < count_of_present_modes; ++i ) {
		if ( present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR ) {	
			chosen_present_mode_index = i;
			break;
		}

		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_KHR ) {
			chosen_present_mode_index = i;
			break;
		}
	}

	return present_modes[chosen_present_mode_index];
}

VkSemaphore
create_vulkan_semaphore_for_image_availability( Vulkan_Context *vulkan_context ) 
{
	VkSemaphoreCreateInfo semaphore_create_info = { 0 };
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkResult result;
	VkSemaphore image_available;

	result = vkCreateSemaphore( vulkan_context->logical_device, &semaphore_create_info, NULL, &image_available );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to create a semaphore for image availability\n" );
		exit( EXIT_FAILURE );
	}

	return image_available;
}

VkSemaphore
create_vulkan_semaphore_for_completion_of_rendering( Vulkan_Context *vulkan_context )
{
	VkSemaphoreCreateInfo semaphore_create_info = { 0 };
	semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkResult result;
	VkSemaphore rendering_complete;

	result = vkCreateSemaphore( vulkan_context->logical_device, &semaphore_create_info, NULL, &rendering_complete );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to create a semaphore for testing rendering completion\n" );
		exit( EXIT_FAILURE );
	}

	return rendering_complete;
}

/* Function does a lot -- here is a breakdown 

 - acquire surface_and_swap_chain_capabilities
	- select number of swap chain images     -- uint32_t
	- select size for swap chain images      -- VkExtent
	- select swap chain usage flags          -- VkImageUsageFlags	
	- select_swap_chain_pre_transforms       -- VkSurfaceTransformFlagBitsKHR
 - acquire supported surface formats
	- select format for swap chain images    -- VkSurfaceFormatKHR	
 - acquire supported present modes 
	- select presentation mode				 -- VkPresentModeKHR
 -create baby's first swap_chain           	 -- VkSwapchainKHR

*/
	
VkSwapchainKHR
create_vulkan_swap_chain( Vulkan_Context *vulkan_context ) 
{
	VkSurfaceCapabilitiesKHR surface_capabilities;
	surface_capabilities = acquire_surface_and_swap_chain_capabilities( vulkan_context );

	uint32_t _desired_number_of_images;
	_desired_number_of_images = select_number_of_swap_chain_images( &surface_capabilities );
	
	VkExtent2D _desired_extent;
	_desired_extent = select_swap_chain_image_size( &surface_capabilities );

	VkImageUsageFlags _desired_usage;
	_desired_usage = select_swap_chain_usage_flags( &surface_capabilities );

	VkSurfaceTransformFlagBitsKHR _desired_pre_transform;
	_desired_pre_transform = select_swap_chain_pre_transforms( &surface_capabilities );

	
	
	VkSurfaceFormatKHR *surface_formats;
	uint32_t count_of_surface_formats;
	surface_formats = acquire_supported_surface_formats( vulkan_context, &count_of_surface_formats );

	VkSurfaceFormatKHR _desired_format;
	_desired_format = select_format_for_swap_chain_images( surface_formats, count_of_surface_formats );



	VkPresentModeKHR *surface_present_modes;
	uint32_t count_of_surface_present_modes;
	surface_present_modes = acquire_supported_present_modes( vulkan_context, &count_of_surface_present_modes );
	
	VkPresentModeKHR _desired_present_mode;
	_desired_present_mode = select_swap_chain_present_mode( surface_present_modes, count_of_surface_present_modes );


	// NOTE: Docs -- what the fuck is this old_swap_chain used for??
	// *set it to NULL -- need to make sure that doesn't fuck me up
	VkSwapchainCreateInfoKHR swap_chain_create_info = { 0 };
	swap_chain_create_info.sType 		         = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swap_chain_create_info.surface 	             = vulkan_context->surface;
	swap_chain_create_info.minImageCount         = _desired_number_of_images;
	swap_chain_create_info.imageFormat           = _desired_format.format;
	swap_chain_create_info.imageColorSpace       = _desired_format.colorSpace;
	swap_chain_create_info.imageExtent           = _desired_extent;
	swap_chain_create_info.imageArrayLayers      = 1;
	swap_chain_create_info.imageUsage            = _desired_usage;
	swap_chain_create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
	swap_chain_create_info.queueFamilyIndexCount = 0;  							// not sure about this one
	swap_chain_create_info.pQueueFamilyIndices   = NULL; 						// or this one too
	swap_chain_create_info.preTransform          = _desired_pre_transform;
	swap_chain_create_info.compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swap_chain_create_info.presentMode		     = _desired_present_mode;
	swap_chain_create_info.clipped			     = VK_TRUE;
	swap_chain_create_info.oldSwapchain          = NULL;

		
	VkResult result;
	VkSwapchainKHR new_swap_chain;
	result = vkCreateSwapchainKHR( vulkan_context->logical_device, &swap_chain_create_info, NULL, &new_swap_chain );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to create a swap chain\n" );
		exit( EXIT_FAILURE );
	}

	free( surface_formats );
	free( surface_present_modes );

	return new_swap_chain;	

}

uint32_t
get_count_of_swap_chain_images( Vulkan_Context *vulkan_context )
{
	VkResult result;
	uint32_t count_of_swap_chain_images;
		
	result = vkGetSwapchainImagesKHR( vulkan_context->logical_device, vulkan_context->swap_chain, &count_of_swap_chain_images, NULL );	
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Could not retrieve the number of swap chain images!\n" );
		exit( EXIT_FAILURE );
	}

	if ( count_of_swap_chain_images == 0 ) {
		fprintf( stdout, "Number of images tied to swap chain is zero\n" );
		exit( EXIT_FAILURE );
	}

	return count_of_swap_chain_images;
}

VkCommandPool
create_vulkan_command_pool( Vulkan_Context *vulkan_context ) 
{
	VkResult result;
	VkCommandPoolCreateInfo command_pool_create_info = { 0 };

	command_pool_create_info.sType 			  = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = vulkan_context->queue_family_index;

	VkCommandPool command_pool;
	result = vkCreateCommandPool( vulkan_context->logical_device, &command_pool_create_info, NULL, &command_pool );

	return command_pool;	
}

VkCommandBuffer *
create_vulkan_command_buffers( Vulkan_Context *vulkan_context ) 
{
	VkResult result;
	VkCommandBufferAllocateInfo command_buffer_allocate_info = { 0 };

	command_buffer_allocate_info.sType 			    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_allocate_info.commandPool 		= vulkan_context->command_pool;
	command_buffer_allocate_info.level       		= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	command_buffer_allocate_info.commandBufferCount = vulkan_context->count_of_command_buffers;

	VkCommandBuffer *command_buffers;
	command_buffers = (VkCommandBuffer *)malloc( vulkan_context->count_of_command_buffers * sizeof (VkCommandBuffer) );
	if ( !command_buffers ) {
		fprintf( stdout, "Unable to allocate space for a new command buffers\n" );
		exit( EXIT_FAILURE );
	}

	result = vkAllocateCommandBuffers( vulkan_context->logical_device, &command_buffer_allocate_info, command_buffers ); 
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to get handles to command buffers\n" );
		exit( EXIT_FAILURE );
	}

	return command_buffers;
}

void 
record_command_buffer( Vulkan_Context *vulkan_context )
{
	VkResult result;
	VkImage *swap_chain_images;

	swap_chain_images = (VkImage *)malloc( vulkan_context->count_of_swap_chain_images * sizeof (VkImage) );
	if ( !swap_chain_images ) {
		fprintf( stdout, "Unable to allocate space to store swap chain image handles\n" );
		exit( EXIT_FAILURE );
	}

	result = vkGetSwapchainImagesKHR( vulkan_context->logical_device, vulkan_context->swap_chain, &vulkan_context->count_of_swap_chain_images, swap_chain_images );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Unable to get handles to swap chain images\n" );
		exit( EXIT_FAILURE );
	}


	VkCommandBufferBeginInfo command_buffer_begin_info = { 0 };
	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	VkClearColorValue clear_color = {
		{ 1.0f, 0.8f, 0.4f, 0.0f }
	};

	VkImageSubresourceRange image_subresource_range = { 0 };
	image_subresource_range.aspectMask   = VK_IMAGE_ASPECT_COLOR_BIT;
	image_subresource_range.baseMipLevel = 1;
	image_subresource_range.layerCount   = 1;

	for ( uint32_t i = 0; i < vulkan_context->count_of_swap_chain_images; ++i ) {

		VkImageMemoryBarrier barrier_from_present_to_clear = { 0 };
		barrier_from_present_to_clear.sType 			  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier_from_present_to_clear.srcAccessMask 	  = VK_ACCESS_MEMORY_READ_BIT;
		barrier_from_present_to_clear.dstAccessMask 	  = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier_from_present_to_clear.oldLayout     	  = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier_from_present_to_clear.newLayout     	  = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier_from_present_to_clear.srcQueueFamilyIndex = vulkan_context->queue_family_index;
		barrier_from_present_to_clear.dstQueueFamilyIndex = vulkan_context->queue_family_index;
		barrier_from_present_to_clear.image               = swap_chain_images[i];
		barrier_from_present_to_clear.subresourceRange    = image_subresource_range;

		VkImageMemoryBarrier barrier_from_clear_to_present = { 0 };
		barrier_from_clear_to_present.sType 			  = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier_from_clear_to_present.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier_from_clear_to_present.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
		barrier_from_clear_to_present.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier_from_clear_to_present.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		barrier_from_clear_to_present.srcQueueFamilyIndex = vulkan_context->queue_family_index;
		barrier_from_clear_to_present.dstQueueFamilyIndex = vulkan_context->queue_family_index;
		barrier_from_clear_to_present.image               = swap_chain_images[i];
		barrier_from_clear_to_present.subresourceRange    = image_subresource_range;

		vkBeginCommandBuffer( vulkan_context->command_buffers[i], &command_buffer_begin_info );
		vkCmdPipelineBarrier( vulkan_context->command_buffers[i], 
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  0, 0, NULL, 0, NULL, 1,
							  &barrier_from_present_to_clear );

		vkCmdClearColorImage( vulkan_context->command_buffers[i], 
							  swap_chain_images[i],
							  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
							  &clear_color,
							  1,
							  &image_subresource_range );

		vkCmdPipelineBarrier( vulkan_context->command_buffers[i],
							  VK_PIPELINE_STAGE_TRANSFER_BIT,
							  VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
							  0, 0, NULL, 0, NULL, 1,
							  &barrier_from_clear_to_present );

		result = vkEndCommandBuffer( vulkan_context->command_buffers[i] );
		if ( result != VK_SUCCESS ) {
			fprintf( stdout, "Could not record command buffers\n" );
			exit( EXIT_FAILURE );
		}

	}

	return;
}

void
draw( Vulkan_Context *vulkan_context )
{
	VkResult result;
	uint32_t image_index;
	result = vkAcquireNextImageKHR( vulkan_context->logical_device, 
									vulkan_context->swap_chain, 
									UINT64_MAX, 
									vulkan_context->image_available,
									VK_NULL_HANDLE,
									&image_index );
	switch ( result ) {
		case VK_SUCCESS:
		case VK_SUBOPTIMAL_KHR: {
		} break;
		
		case VK_ERROR_OUT_OF_DATE_KHR: {
			fprintf( stdout, "Fuck my tiny b-hole\n" );
		} break;

		default: {
			fprintf( stdout, "unable to swap image\n" );
			exit( EXIT_FAILURE );
		} break;
	}


	VkPipelineStageFlags wait_dst_stage_mask;
	wait_dst_stage_mask = VK_PIPELINE_STAGE_TRANSFER_BIT;
	
	VkSubmitInfo submit_info = { 0 };
	submit_info.sType 			     = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.waitSemaphoreCount   = 1;
	submit_info.pWaitSemaphores      = &vulkan_context->image_available;
	submit_info.pWaitDstStageMask    = &wait_dst_stage_mask;
	submit_info.commandBufferCount   = 1;
	submit_info.pCommandBuffers      = &vulkan_context->command_buffers[image_index];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores    = &vulkan_context->rendering_complete;

	result = vkQueueSubmit( vulkan_context->present_queue, 1, &submit_info, VK_NULL_HANDLE );
	if ( result != VK_SUCCESS ) {
		fprintf( stdout, "Fuck..unable to draw\n" );
		exit( EXIT_FAILURE );
	}

	VkPresentInfoKHR present_info = { 0 };

	present_info.sType 				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores 	= &vulkan_context->rendering_complete;
	present_info.swapchainCount     = 1;
	present_info.pSwapchains		= &vulkan_context->swap_chain;
	present_info.pImageIndices      = &image_index;
	present_info.pResults			= NULL;

	result = vkQueuePresentKHR( vulkan_context->present_queue, &present_info );
	switch ( result ) {
		case VK_SUCCESS: {
		} break;

		case VK_ERROR_OUT_OF_DATE_KHR:
		case VK_SUBOPTIMAL_KHR: {
			fprintf( stdout, "Fuck just put a color on the screen\n" );
			exit( EXIT_FAILURE );
		}
		
		default: {
			fprintf( stdout, "Shit is fucked up\n" );
			exit( EXIT_FAILURE );
		} break;
	}
}

LRESULT CALLBACK
win32_main_window_callback( HWND window_handle, UINT window_message, WPARAM w_param, LPARAM l_param ) 
{
	/* Need to handle WM_SIZE -- something with the queues or buffers, need to read the tutorial again */
	LRESULT result = 0;
	switch (window_message) {
		
		case WM_PAINT: {
			record_command_buffer( &vulkan_context );	
			draw( &vulkan_context );
		} break;

		case WM_QUIT:
		case WM_CLOSE: {
			DestroyWindow( window_handle );
			window_open = false;
		} break;

		default: {
			result = DefWindowProc( window_handle, window_message, w_param, l_param );
		} break;
	}

	return result;
}

int CALLBACK 
WinMain( HINSTANCE windows_instance, HINSTANCE previous_instance, LPSTR command_line_args, int	show_window_options )
{
	#define WINDOW_WIDTH  640
	#define WINDOW_HEIGHT 480

	WNDCLASS window_class = {0}; // initializes all members to zero

	// NOTE(Kevin): If we want to add a cursor need to add the hCursor class
	window_class.style  = CS_HREDRAW | CS_OWNDC | CS_VREDRAW;   				// CS_OWNDC -- required for OpenGl context
	window_class.lpfnWndProc = win32_main_window_callback;
	window_class.hInstance = windows_instance;
	window_class.hCursor = NULL;                 	
	window_class.lpszClassName = "Depth_Main_Window";

	if ( !RegisterClass( &window_class ) ) {	
		get_last_error_as_string( "RegisterClass: " );
		exit( EXIT_FAILURE );
	}

	HWND window_handle;
	window_handle = CreateWindow( window_class.lpszClassName,
  					  	          "Depth",
  					  	          WS_OVERLAPPEDWINDOW | WS_VISIBLE,
  					  	          CW_USEDEFAULT, CW_USEDEFAULT,
  					  	          WINDOW_WIDTH, WINDOW_HEIGHT,
				      	          0, 0, windows_instance, 0 
				      	        );

	 if ( !window_handle ) {	
 		get_last_error_as_string( "CreateWindow: " );
 		exit( EXIT_FAILURE );
 	}

#if 0
	AllocConsole();
	freopen("CONOUT$", "w", stdout );
#endif

	// ONCE DONE WITH CLEANUP -- REMOVE VkResult op_result
	VkResult result;
	
	HMODULE vulkan_library_handle;
	vulkan_library_handle = load_vulkan_library();
	
	load_vulkan_global_functions( vulkan_library_handle );

	verify_instance_supports_required_extensions( &vulkan_context );

	vulkan_context.instance = create_vulkan_instance( &vulkan_context );

	load_vulkan_instance_functions( &vulkan_context );
	load_vulkan_instance_extension_functions( &vulkan_context );

	vulkan_context.surface = create_vulkan_surface( &vulkan_context, windows_instance, window_handle );

	VkPhysicalDevice *physical_devices;
	uint32_t physical_device_count;
	physical_devices = find_vulkan_enabled_physical_devices( &vulkan_context, &physical_device_count );

	vulkan_context.physical_device = find_discrete_graphics_card( physical_devices, physical_device_count );
	
	verify_physical_device_supports_required_extensions( vulkan_context.physical_device );
	vulkan_context.queue_family_index = find_queue_family_with_queues_supporting_graphics_and_presentation( &vulkan_context );
		
	vulkan_context.logical_device = create_vulkan_logical_device( &vulkan_context );

	load_vulkan_device_functions( &vulkan_context );
	load_vulkan_device_extension_functions( &vulkan_context );

	vkGetDeviceQueue( vulkan_context.logical_device, vulkan_context.queue_family_index, 0, &vulkan_context.graphics_queue );
	vkGetDeviceQueue( vulkan_context.logical_device, vulkan_context.queue_family_index, 0, &vulkan_context.present_queue );

	vulkan_context.image_available    = create_vulkan_semaphore_for_image_availability( &vulkan_context );
	vulkan_context.rendering_complete = create_vulkan_semaphore_for_completion_of_rendering( &vulkan_context );

	vulkan_context.swap_chain 				  = create_vulkan_swap_chain( &vulkan_context );		
	vulkan_context.count_of_swap_chain_images = get_count_of_swap_chain_images( &vulkan_context );
	vulkan_context.count_of_command_buffers   = vulkan_context.count_of_swap_chain_images;
	vulkan_context.command_pool 			  = create_vulkan_command_pool( &vulkan_context );
	vulkan_context.command_buffers			  = create_vulkan_command_buffers( &vulkan_context );
	
	while ( window_open ) {
		MSG window_messages;
		while ( PeekMessage( &window_messages, window_handle, 0, 0, PM_REMOVE ) ) {
			TranslateMessage( &window_messages );
			DispatchMessage( &window_messages );
		}
	}

	fprintf( stdout, "Mission success!!!\n" );

// 
//  Doesn't free everything it should, just got tired of tracking through 1100 lines of SETUP CODE ARRRGGGH!!!
//
	free( physical_devices );
	vkDeviceWaitIdle( vulkan_context.logical_device );
	vkDestroyDevice( vulkan_context.logical_device, NULL );
	vkDestroyInstance( vulkan_context.instance, NULL );
	FreeLibrary( vulkan_library_handle );

	return 0;
}
			 

