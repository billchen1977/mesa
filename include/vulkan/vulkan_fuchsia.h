#ifndef VULKAN_FUCHSIA_H_
#define VULKAN_FUCHSIA_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

/*
** Copyright (c) 2015-2018 The Khronos Group Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/*
** This header is generated from the Khronos Vulkan XML API Registry.
**
*/


#define VK_KHR_external_memory_fuchsia 1
#define VK_KHR_EXTERNAL_MEMORY_FUCHSIA_SPEC_VERSION 1
#define VK_KHR_EXTERNAL_MEMORY_FUCHSIA_EXTENSION_NAME "VK_KHR_external_memory_fuchsia"

typedef struct VkImportMemoryFuchsiaHandleInfoKHR {
    VkStructureType                       sType;
    const void*                           pNext;
    VkExternalMemoryHandleTypeFlagBits    handleType;
    uint32_t                              handle;
} VkImportMemoryFuchsiaHandleInfoKHR;

typedef struct VkMemoryFuchsiaHandlePropertiesKHR {
    VkStructureType    sType;
    void*              pNext;
    uint32_t           memoryTypeBits;
} VkMemoryFuchsiaHandlePropertiesKHR;

typedef struct VkMemoryGetFuchsiaHandleInfoKHR {
    VkStructureType                       sType;
    const void*                           pNext;
    VkDeviceMemory                        memory;
    VkExternalMemoryHandleTypeFlagBits    handleType;
} VkMemoryGetFuchsiaHandleInfoKHR;


typedef VkResult (VKAPI_PTR *PFN_vkGetMemoryFuchsiaHandleKHR)(VkDevice device, const VkMemoryGetFuchsiaHandleInfoKHR* pGetFuchsiaHandleInfo, uint32_t* pFuchsiaHandle);
typedef VkResult (VKAPI_PTR *PFN_vkGetMemoryFuchsiaHandlePropertiesKHR)(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, uint32_t fuchsiaHandle, VkMemoryFuchsiaHandlePropertiesKHR* pMemoryFuchsiaHandleProperties);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFuchsiaHandleKHR(
    VkDevice                                    device,
    const VkMemoryGetFuchsiaHandleInfoKHR*      pGetFuchsiaHandleInfo,
    uint32_t*                                   pFuchsiaHandle);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryFuchsiaHandlePropertiesKHR(
    VkDevice                                    device,
    VkExternalMemoryHandleTypeFlagBits          handleType,
    uint32_t                                    fuchsiaHandle,
    VkMemoryFuchsiaHandlePropertiesKHR*         pMemoryFuchsiaHandleProperties);
#endif

#define VK_KHR_external_semaphore_fuchsia 1
#define VK_KHR_EXTERNAL_SEMAPHORE_FUCHSIA_SPEC_VERSION 1
#define VK_KHR_EXTERNAL_SEMAPHORE_FUCHSIA_EXTENSION_NAME "VK_KHR_external_semaphore_fuchsia"

typedef struct VkImportSemaphoreFuchsiaHandleInfoKHR {
    VkStructureType                          sType;
    const void*                              pNext;
    VkSemaphore                              semaphore;
    VkSemaphoreImportFlags                   flags;
    VkExternalSemaphoreHandleTypeFlagBits    handleType;
    uint32_t                                 handle;
} VkImportSemaphoreFuchsiaHandleInfoKHR;

typedef struct VkSemaphoreGetFuchsiaHandleInfoKHR {
    VkStructureType                          sType;
    const void*                              pNext;
    VkSemaphore                              semaphore;
    VkExternalSemaphoreHandleTypeFlagBits    handleType;
} VkSemaphoreGetFuchsiaHandleInfoKHR;


typedef VkResult (VKAPI_PTR *PFN_vkImportSemaphoreFuchsiaHandleKHR)(VkDevice device, const VkImportSemaphoreFuchsiaHandleInfoKHR* pImportSemaphoreFuchsiaHandleInfo);
typedef VkResult (VKAPI_PTR *PFN_vkGetSemaphoreFuchsiaHandleKHR)(VkDevice device, const VkSemaphoreGetFuchsiaHandleInfoKHR* pGetFuchsiaHandleInfo, uint32_t* pFuchsiaHandle);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreFuchsiaHandleKHR(
    VkDevice                                    device,
    const VkImportSemaphoreFuchsiaHandleInfoKHR* pImportSemaphoreFuchsiaHandleInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreFuchsiaHandleKHR(
    VkDevice                                    device,
    const VkSemaphoreGetFuchsiaHandleInfoKHR*   pGetFuchsiaHandleInfo,
    uint32_t*                                   pFuchsiaHandle);
#endif

#define VK_FUCHSIA_imagepipe_surface 1
#define VK_FUCHSIA_IMAGEPIPE_SURFACE_SPEC_VERSION 1
#define VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME "VK_FUCHSIA_imagepipe_surface"

typedef VkFlags VkImagePipeSurfaceCreateFlagsFUCHSIA;

typedef struct VkImagePipeSurfaceCreateInfoFUCHSIA {
    VkStructureType                         sType;
    const void*                             pNext;
    VkImagePipeSurfaceCreateFlagsFUCHSIA    flags;
    zx_handle_t                             imagePipeHandle;
} VkImagePipeSurfaceCreateInfoFUCHSIA;


typedef VkResult (VKAPI_PTR *PFN_vkCreateImagePipeSurfaceFUCHSIA)(VkInstance instance, const VkImagePipeSurfaceCreateInfoFUCHSIA* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateImagePipeSurfaceFUCHSIA(
    VkInstance                                  instance,
    const VkImagePipeSurfaceCreateInfoFUCHSIA*  pCreateInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkSurfaceKHR*                               pSurface);
#endif

#define VK_GOOGLE_image_usage_scanout 1
#define VK_GOOGLE_IMAGE_USAGE_SCANOUT_SPEC_VERSION 1
#define VK_GOOGLE_IMAGE_USAGE_SCANOUT_EXTENSION_NAME "VK_GOOGLE_image_usage_scanout"

#define VK_FUCHSIA_buffer_collection 1
VK_DEFINE_NON_DISPATCHABLE_HANDLE(VkBufferCollectionFUCHSIA)

#define VK_FUCHSIA_BUFFER_COLLECTION_SPEC_VERSION 1
#define VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME "VK_FUCHSIA_buffer_collection"

typedef struct VkBufferCollectionCreateInfoFUCHSIA {
    VkStructureType    sType;
    const void*        pNext;
    zx_handle_t        collectionToken;
} VkBufferCollectionCreateInfoFUCHSIA;

typedef struct VkFuchsiaImageFormatFUCHSIA {
    VkStructureType    sType;
    const void*        pNext;
    const void*        imageFormat;
    uint32_t           imageFormatSize;
} VkFuchsiaImageFormatFUCHSIA;


typedef VkResult (VKAPI_PTR *PFN_vkCreateBufferCollectionFUCHSIA)(VkDevice device, const VkBufferCollectionCreateInfoFUCHSIA* pImportInfo, const VkAllocationCallbacks* pAllocator, VkBufferCollectionFUCHSIA* pCollection);
typedef VkResult (VKAPI_PTR *PFN_vkSetBufferCollectionConstraintsFUCHSIA)(VkDevice device, VkBufferCollectionFUCHSIA collection, const VkImageCreateInfo* pImageInfo);
typedef void (VKAPI_PTR *PFN_vkDestroyBufferCollectionFUCHSIA)(VkDevice device, VkBufferCollectionFUCHSIA collection, const VkAllocationCallbacks* pAllocator);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkCreateBufferCollectionFUCHSIA(
    VkDevice                                    device,
    const VkBufferCollectionCreateInfoFUCHSIA*  pImportInfo,
    const VkAllocationCallbacks*                pAllocator,
    VkBufferCollectionFUCHSIA*                  pCollection);

VKAPI_ATTR VkResult VKAPI_CALL vkSetBufferCollectionConstraintsFUCHSIA(
    VkDevice                                    device,
    VkBufferCollectionFUCHSIA                   collection,
    const VkImageCreateInfo*                    pImageInfo);

VKAPI_ATTR void VKAPI_CALL vkDestroyBufferCollectionFUCHSIA(
    VkDevice                                    device,
    VkBufferCollectionFUCHSIA                   collection,
    const VkAllocationCallbacks*                pAllocator);
#endif


#define VK_FUCHSIA_external_memory 1
#define VK_FUCHSIA_EXTERNAL_MEMORY_SPEC_VERSION 1
#define VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME "VK_FUCHSIA_external_memory"

typedef struct VkImportMemoryZirconHandleInfoFUCHSIA {
    VkStructureType                       sType;
    const void*                           pNext;
    VkExternalMemoryHandleTypeFlagBits    handleType;
    zx_handle_t                           handle;
} VkImportMemoryZirconHandleInfoFUCHSIA;

typedef struct VkMemoryZirconHandlePropertiesFUCHSIA {
    VkStructureType    sType;
    void*              pNext;
    zx_handle_t        memoryTypeBits;
} VkMemoryZirconHandlePropertiesFUCHSIA;

typedef struct VkMemoryGetZirconHandleInfoFUCHSIA {
    VkStructureType                       sType;
    const void*                           pNext;
    VkDeviceMemory                        memory;
    VkExternalMemoryHandleTypeFlagBits    handleType;
} VkMemoryGetZirconHandleInfoFUCHSIA;


typedef VkResult (VKAPI_PTR *PFN_vkGetMemoryZirconHandleFUCHSIA)(VkDevice device, const VkMemoryGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, zx_handle_t* pZirconHandle);
typedef VkResult (VKAPI_PTR *PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA)(VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType, zx_handle_t ZirconHandle, VkMemoryZirconHandlePropertiesFUCHSIA* pMemoryZirconHandleProperties);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryZirconHandleFUCHSIA(
    VkDevice                                    device,
    const VkMemoryGetZirconHandleInfoFUCHSIA*   pGetZirconHandleInfo,
    zx_handle_t*                                pZirconHandle);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryZirconHandlePropertiesFUCHSIA(
    VkDevice                                    device,
    VkExternalMemoryHandleTypeFlagBits          handleType,
    zx_handle_t                                 ZirconHandle,
    VkMemoryZirconHandlePropertiesFUCHSIA*      pMemoryZirconHandleProperties);
#endif

#define VK_FUCHSIA_external_semaphore 1
#define VK_FUCHSIA_EXTERNAL_SEMAPHORE_SPEC_VERSION 1
#define VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME "VK_FUCHSIA_external_semaphore"

typedef struct VkImportSemaphoreZirconHandleInfoFUCHSIA {
    VkStructureType                          sType;
    const void*                              pNext;
    VkSemaphore                              semaphore;
    VkSemaphoreImportFlags                   flags;
    VkExternalSemaphoreHandleTypeFlagBits    handleType;
    zx_handle_t                              handle;
} VkImportSemaphoreZirconHandleInfoFUCHSIA;

typedef struct VkSemaphoreGetZirconHandleInfoFUCHSIA {
    VkStructureType                          sType;
    const void*                              pNext;
    VkSemaphore                              semaphore;
    VkExternalSemaphoreHandleTypeFlagBits    handleType;
} VkSemaphoreGetZirconHandleInfoFUCHSIA;


typedef VkResult (VKAPI_PTR *PFN_vkImportSemaphoreZirconHandleFUCHSIA)(VkDevice device, const VkImportSemaphoreZirconHandleInfoFUCHSIA* pImportSemaphoreZirconHandleInfo);
typedef VkResult (VKAPI_PTR *PFN_vkGetSemaphoreZirconHandleFUCHSIA)(VkDevice device, const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo, zx_handle_t* pZirconHandle);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreZirconHandleFUCHSIA(
    VkDevice                                    device,
    const VkImportSemaphoreZirconHandleInfoFUCHSIA* pImportSemaphoreZirconHandleInfo);

VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreZirconHandleFUCHSIA(
    VkDevice                                    device,
    const VkSemaphoreGetZirconHandleInfoFUCHSIA* pGetZirconHandleInfo,
    zx_handle_t*                                pZirconHandle);
#endif

#ifdef __cplusplus
}
#endif

#endif