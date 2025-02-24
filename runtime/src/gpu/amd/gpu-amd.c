/*
 * Copyright 2020-2025 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.  *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAS_GPU_LOCALE

#include "sys_basic.h"
#include "chplrt.h"
#include "chplsys.h"
#include "chpl-mem.h"
#include "chpl-gpu.h"
#include "chpl-gpu-impl.h"
#include "chpl-tasks.h"
#include "error.h"
#include "chplcgfns.h"
#include "chpl-env-gen.h"
#include "chpl-linefile-support.h"
#include "gpu/amd/rocm-utils.h"
#include "chpl-topo.h"

#include <assert.h>

#ifndef __HIP_PLATFORM_AMD__
#define __HIP_PLATFORM_AMD__
#endif
#include <hip/hip_runtime.h>
#include <hip/hip_runtime_api.h>
#include <hip/hip_common.h>

// this is compiler-generated
extern const char* chpl_gpuBinary;
extern const uint64_t chpl_gpuBinarySize;

static int numAllDevices = -1;
static int numDevices = -1;
static int *dev_pid_to_lid_table = NULL;
static hipDevice_t *dev_lid_to_pid_table = NULL;

// array indexed by device ID (we load the same module once for each GPU).
static hipModule_t *chpl_gpu_rocm_modules;

static int *deviceClockRates;

static inline
void* chpl_gpu_load_module(const char* fatbin_data, const uint64_t fatbin_size) {
  hipModule_t rocm_module;

  const char* buffer = fatbin_data;
  chpl_bool has_huge_pages = chpl_getHeapPageSize() > chpl_getSysPageSize();
  if (has_huge_pages) {
    // ROCm and huge pages don't play well together.
    // Make a temporary copy of the fatbin
    buffer = chpl_malloc(fatbin_size);
    memcpy((void*)buffer, fatbin_data, fatbin_size);
  }

  ROCM_CALL(hipModuleLoadData(&rocm_module, buffer));
  assert(rocm_module);

  if (has_huge_pages) chpl_free((void*)buffer);

  return (void*)rocm_module;
}

// Maps the "physical" device ID used by the HIP library to the "logical"
// device ID used by this locale. The two may differ due to co-locales.
// Logical device IDs start with zero in each co-locale and are equal to the
// sublocale ID. Physical device IDs are the same for all co-locales on the
// machine.

static int dev_pid_to_lid(int32_t dev_pid) {
  assert((dev_pid >= 0) && (dev_pid < numAllDevices));
  int dev_lid = dev_pid_to_lid_table[dev_pid];
  assert((dev_lid >= 0) && (dev_lid < numDevices));
  return dev_lid;
}

// Maps a logical device ID to physical device ID.

static int dev_lid_to_pid(int dev_lid) {
  assert((dev_lid >= 0) && (dev_lid < numDevices));
  int dev_pid = dev_lid_to_pid_table[dev_lid];
  assert((dev_pid >= 0) && (dev_pid < numAllDevices));
  return dev_pid;
}

static void switch_context(int dev_lid) {
  int dev_pid = dev_lid_to_pid(dev_lid);
  ROCM_CALL(hipSetDevice(dev_pid));
}

void chpl_gpu_impl_use_device(c_sublocid_t dev_lid) {
  switch_context(dev_lid);
}

static hipModule_t get_module(void) {
  hipDevice_t device;
  hipModule_t module;

  ROCM_CALL(hipGetDevice(&device));
  int dev_lid = dev_pid_to_lid((int32_t) device);
  module = chpl_gpu_rocm_modules[dev_lid];
  return module;
}

extern c_nodeid_t chpl_nodeID;

static void chpl_gpu_impl_set_globals(c_sublocid_t dev_lid, hipModule_t module) {
  hipDeviceptr_t ptr = NULL;;
  size_t glob_size;

  chpl_gpu_impl_load_global("chpl_nodeID", (void**)&ptr, &glob_size);

  if (ptr) {
    assert(glob_size == sizeof(c_nodeid_t));

    // chpl_gpu_impl_copy_host_to_device performs a validation using
    // hipPointerGetAttributes. However, apparently, that's not something you
    // should call on pointers returned from hipModuleGetGlobal. Just perform
    // the copy directly.
    //
    // The validation only happens when built with assertions (commonly
    // enabled by CHPL_DEVELOPER), and chpl_gpu_impl_copy_host_to_device
    // only causes issues in that case.
    ROCM_CALL(hipMemcpyHtoD(ptr, (void*)&chpl_nodeID, glob_size));
  }
}


void chpl_gpu_impl_load_global(const char* global_name, void** ptr,
                               size_t* size) {
  hipModule_t module = get_module();

  //
  // Engin: The AMDGPU backend seems to optimize globals away when they are not
  // used.  So, we should not error out if we can't find its definition. We can
  // look into making sure that it remains in the module, which feels a bit
  // safer, admittedly. Note also that this is the only diff between nvidia and
  // amd implementations in terms of adjusting chpl_nodeID.
  int err = hipModuleGetGlobal((hipDeviceptr_t*)ptr, size, module, global_name);
  if (err == hipErrorNotFound) {
    return;
  }
  ROCM_CALL(err);
}

void* chpl_gpu_impl_load_function(const char* kernel_name) {
  hipFunction_t function;
  hipModule_t module = get_module();

  ROCM_CALL(hipModuleGetFunction(&function, module, kernel_name));
  assert(function);

  return (void*)function;
}

void chpl_gpu_impl_begin_init(int* num_all_devices) {
  ROCM_CALL(hipInit(0));
  ROCM_CALL(hipGetDeviceCount(&numAllDevices));
  *num_all_devices = numAllDevices;
}

static hipDevice_t ith_device(int i) {
#if ROCM_VERSION_MAJOR >= 6
  return i;
#else
  hipDevice_t device;
  ROCM_CALL(hipDeviceGet(&device, i));
  return device;
#endif
}

void chpl_gpu_impl_collect_topo_addr_info(chpl_topo_pci_addr_t* into,
                                          int device_num) {
  hipDevice_t hipDevice = ith_device(device_num);
  int domain, bus, device;
  int rc = hipDeviceGetAttribute(&domain, hipDeviceAttributePciDomainID,
                                 hipDevice);
  if (rc == hipErrorInvalidValue) {
    // hipDeviceGetAttribute for hipDeviceAttributePciDomainID fails
    // on some (all?) platforms. Assume the domain is 0 and carry on.
    domain = 0;
  } else {
    ROCM_CALL(rc);
  }
  ROCM_CALL(hipDeviceGetAttribute(&bus, hipDeviceAttributePciBusId,
                                  hipDevice));
  ROCM_CALL(hipDeviceGetAttribute(&device, hipDeviceAttributePciDeviceId,
                                  hipDevice));
  into->domain = (uint8_t) domain;
  into->bus = (uint8_t) bus;
  into->device = (uint8_t) device;
  into->function = 0;
}

void chpl_gpu_impl_setup_with_device_count(int num_my_devices) {
  // Allocate the GPU data structures. Note that the HIP API, specifically
  // hipGetDevice, returns the global device ID so we need
  // dev_pid_to_lid_table to map from the global device ID to the logical
  // device ID.

  numDevices = num_my_devices;
  chpl_gpu_rocm_modules = chpl_malloc(sizeof(hipModule_t)*numDevices);
  deviceClockRates = chpl_malloc(sizeof(int)*numDevices);
  dev_lid_to_pid_table = chpl_malloc(sizeof(int) * numDevices);
  dev_pid_to_lid_table = chpl_malloc(sizeof(int) * numAllDevices);

  for (int i = 0; i < numDevices; i++) {
    chpl_gpu_rocm_modules[i] = NULL;
    dev_lid_to_pid_table[i] = -1;
  }

  for (int i = 0; i < numAllDevices; i++) {
    dev_pid_to_lid_table[i] = -1;
  }
}

void chpl_gpu_impl_setup_device(int my_index, int global_index) {
  hipDevice_t device = ith_device(global_index);
#if ROCM_VERSION_MAJOR >= 6
  ROCM_CALL(hipSetDevice(device));
  ROCM_CALL(hipSetDeviceFlags(hipDeviceScheduleBlockingSync))
#else
  hipCtx_t context;
  ROCM_CALL(hipDevicePrimaryCtxSetFlags(device, hipDeviceScheduleBlockingSync));
  ROCM_CALL(hipDevicePrimaryCtxRetain(&context, device));

  ROCM_CALL(hipSetDevice(device));
#endif
  hipModule_t module = chpl_gpu_load_module(chpl_gpuBinary, chpl_gpuBinarySize);
  chpl_gpu_rocm_modules[my_index] = module;

  hipDeviceGetAttribute(&deviceClockRates[my_index],
                        hipDeviceAttributeClockRate, device);

  // map array indices (relative device numbers) to global device IDs
  dev_lid_to_pid_table[my_index] = device;
  dev_pid_to_lid_table[device] = my_index;
  chpl_gpu_impl_set_globals(my_index, module);
}

bool chpl_gpu_impl_is_device_ptr(const void* ptr) {
  if (!ptr) return false;
  hipPointerAttribute_t res;
  hipError_t ret_val = hipPointerGetAttributes(&res, (hipDeviceptr_t)ptr);

  if (ret_val != hipSuccess) {
    if (ret_val == hipErrorInvalidValue ||
        ret_val == hipErrorNotInitialized ||
        ret_val == hipErrorDeinitialized) {
      return false;
    }
    else {
      ROCM_CALL(ret_val);
    }
  }

#if ROCM_VERSION_MAJOR >= 6
  // TODO: is this right?
  return res.type != hipMemoryTypeUnregistered;
#else
  return true;
#endif
}

bool chpl_gpu_impl_is_host_ptr(const void* ptr) {
  if (!ptr) return false;
  hipPointerAttribute_t res;
  hipError_t ret_val = hipPointerGetAttributes(&res, (hipDeviceptr_t)ptr);

  if (ret_val != hipSuccess) {
    if (ret_val == hipErrorInvalidValue ||
        ret_val == hipErrorNotInitialized ||
        ret_val == hipErrorDeinitialized) {
      return true;
    }
    else {
      ROCM_CALL(ret_val);
    }
  }
  else {
#if ROCM_VERSION_MAJOR >= 6
    return res.type == hipMemoryTypeHost || res.type == hipMemoryTypeUnregistered;
#else
    return res.memoryType == hipMemoryTypeHost;
#endif
  }

  return true;
}

void chpl_gpu_impl_launch_kernel(void* kernel,
                                 int grd_dim_x, int grd_dim_y, int grd_dim_z,
                                 int blk_dim_x, int blk_dim_y, int blk_dim_z,
                                 void* stream, void** kernel_params) {
  assert(kernel);

  ROCM_CALL(hipModuleLaunchKernel((hipFunction_t)kernel,
                                  grd_dim_x, grd_dim_y, grd_dim_z,
                                  blk_dim_x, blk_dim_y, blk_dim_z,
                                  0,  // shared memory in bytes
                                  stream,  // stream ID
                                  (void**)kernel_params,
                                  NULL));  // extra options
}

void* chpl_gpu_impl_memset(void* addr, const uint8_t val, size_t n,
                           void* stream) {
  assert(chpl_gpu_is_device_ptr(addr));

  ROCM_CALL(hipMemsetD8Async((hipDeviceptr_t)addr, (unsigned int)val, n,
                              (hipStream_t)stream));

  return addr;
}


void chpl_gpu_impl_copy_device_to_host(void* dst, const void* src, size_t n,
                                       void* stream) {
  assert(chpl_gpu_is_device_ptr(src));

  ROCM_CALL(hipMemcpyDtoHAsync(dst, (hipDeviceptr_t)src, n,
                               (hipStream_t)stream));
}

void chpl_gpu_impl_copy_host_to_device(void* dst, const void* src, size_t n,
                                       void* stream) {
  assert(chpl_gpu_is_device_ptr(dst));

  ROCM_CALL(hipMemcpyHtoDAsync((hipDeviceptr_t)dst, (void *)src, n,
                               (hipStream_t)stream));
}

void chpl_gpu_impl_copy_device_to_device(void* dst, const void* src, size_t n,
                                         void* stream) {
  assert(chpl_gpu_is_device_ptr(dst) && chpl_gpu_is_device_ptr(src));

  ROCM_CALL(hipMemcpyDtoDAsync((hipDeviceptr_t)dst, (hipDeviceptr_t)src, n,
                               (hipStream_t)stream));
}


void* chpl_gpu_impl_comm_async(void *dst, void *src, size_t n) {
  hipStream_t stream;
  hipStreamCreateWithFlags(&stream, hipStreamNonBlocking);
  hipMemcpyAsync((hipDeviceptr_t)dst, (hipDeviceptr_t)src, n, hipMemcpyDefault, stream);
  return stream;
}

void chpl_gpu_impl_comm_wait(void *stream) {
  hipStreamSynchronize((hipStream_t)stream);
  hipStreamDestroy((hipStream_t)stream);
}

void* chpl_gpu_impl_mem_array_alloc(size_t size) {
  assert(size>0);

  hipDeviceptr_t ptr = 0;

#ifdef CHPL_GPU_MEM_STRATEGY_ARRAY_ON_DEVICE
    // hip doesn't have stream-ordered memory allocator (no hipMallocAsync)
    ROCM_CALL(hipMalloc(&ptr, size));
#else
    ROCM_CALL(hipMallocManaged(&ptr, size, hipMemAttachGlobal));
#endif

  return (void*)ptr;
}

void* chpl_gpu_impl_mem_alloc(size_t size) {
#ifdef CHPL_GPU_MEM_STRATEGY_ARRAY_ON_DEVICE
  void* ptr = 0;
  ROCM_CALL(hipHostMalloc(&ptr, size, 0));
#else
  hipDeviceptr_t ptr = 0;
  ROCM_CALL(hipMallocManaged(&ptr, size, hipMemAttachGlobal));
#endif
  assert(ptr!=0);

  return (void*)ptr;
}

void chpl_gpu_impl_mem_free(void* memAlloc) {
  if (memAlloc != NULL) {
    // see note in chpl_gpu_mem_free
    hipPointerAttribute_t res;
    ROCM_CALL(hipPointerGetAttributes(&res, memAlloc));
    int dev_lid = dev_pid_to_lid(res.device);
    switch_context(dev_lid);

    assert(chpl_gpu_is_device_ptr(memAlloc));
#ifdef CHPL_GPU_MEM_STRATEGY_ARRAY_ON_DEVICE
    if (chpl_gpu_impl_is_host_ptr(memAlloc)) {
      ROCM_CALL(hipHostFree(memAlloc));
    }
    else {
#endif
    // hip doesn't have stream-ordered memory allocator (no hipFreeAsync)
    ROCM_CALL(hipFree((hipDeviceptr_t)memAlloc));
#ifdef CHPL_GPU_MEM_STRATEGY_ARRAY_ON_DEVICE
    }
#endif
  }
}

void chpl_gpu_impl_hostmem_register(void *memAlloc, size_t size) {
  // The ROCM driver uses DMA to transfer page-locked memory to the GPU; if
  // memory is not page-locked it must first be transferred into a page-locked
  // buffer, which degrades performance. So in the array_on_device mode we
  // choose to page-lock such memory even if it's on the host-side.
  #ifdef CHPL_GPU_MEM_STRATEGY_ARRAY_ON_DEVICE
  ROCM_CALL(hipHostRegister(memAlloc, size, hipHostRegisterPortable));
  #endif
}

// This can be used for proper reallocation
size_t chpl_gpu_impl_get_alloc_size(void* ptr) {
  hipDeviceptr_t base;
  size_t size;
  ROCM_CALL(hipMemGetAddressRange(&base, &size, (hipDeviceptr_t)ptr));

  return size;
}

unsigned int chpl_gpu_device_clock_rate(int32_t devNum) {
  return (unsigned int)deviceClockRates[devNum];
}

bool chpl_gpu_impl_can_access_peer(int dev_lid1, int dev_lid2) {
  int p2p;
  int dev_pid1 = dev_lid_to_pid(dev_lid1);
  int dev_pid2 = dev_lid_to_pid(dev_lid2);
  ROCM_CALL(hipDeviceCanAccessPeer(&p2p, dev_pid1, dev_pid2));
  return p2p != 0;
}

void chpl_gpu_impl_set_peer_access(int dev_lid1, int dev_lid2, bool enable) {
  int dev_pid1 = dev_lid_to_pid(dev_lid1);
  int dev_pid2 = dev_lid_to_pid(dev_lid2);
  ROCM_CALL(hipSetDevice(dev_pid1));
  if(enable) {
    ROCM_CALL(hipDeviceEnablePeerAccess(dev_pid2, 0));
  } else {
    ROCM_CALL(hipDeviceDisablePeerAccess(dev_pid2));
  }
}

void chpl_gpu_impl_synchronize(void) {
  ROCM_CALL(hipDeviceSynchronize());
}

bool chpl_gpu_impl_stream_supported(void) {
  return true;
}

void* chpl_gpu_impl_stream_create(void) {
  hipStream_t stream;
  ROCM_CALL(hipStreamCreateWithFlags(&stream, hipStreamDefault));
  return (void*) stream;
}

void chpl_gpu_impl_stream_destroy(void* stream) {
  if (stream) {
    ROCM_CALL(hipStreamDestroy((hipStream_t)stream));
  }
}

bool chpl_gpu_impl_stream_ready(void* stream) {
  if (stream) {
    hipError_t res = hipStreamQuery(stream);
    if (res == hipErrorNotReady) {
      return false;
    }
    ROCM_CALL(res);
  }
  return true;
}

void chpl_gpu_impl_stream_synchronize(void* stream) {
  if (stream) {
    ROCM_CALL(hipStreamSynchronize(stream));
  }
}

void* chpl_gpu_impl_host_register(void* var, size_t size) {
  ROCM_CALL(hipHostRegister(var, size, hipHostRegisterPortable));
  void *dev_var;
  ROCM_CALL(hipHostGetDevicePointer(&dev_var, var, 0));
  return dev_var;
}

void chpl_gpu_impl_host_unregister(void* var) {
  ROCM_CALL(hipHostUnregister(var));
}

void chpl_gpu_impl_name(int dev_lid, char *resultBuffer, int bufferSize) {
  int dev_pid = dev_lid_to_pid(dev_lid);
  ROCM_CALL(hipDeviceGetName(resultBuffer, bufferSize, dev_pid));
}

const int CHPL_GPU_ATTRIBUTE__MAX_THREADS_PER_BLOCK = hipDeviceAttributeMaxThreadsPerBlock;
const int CHPL_GPU_ATTRIBUTE__MAX_BLOCK_DIM_X = hipDeviceAttributeMaxBlockDimX;
const int CHPL_GPU_ATTRIBUTE__MAX_BLOCK_DIM_Y = hipDeviceAttributeMaxBlockDimY;
const int CHPL_GPU_ATTRIBUTE__MAX_BLOCK_DIM_Z = hipDeviceAttributeMaxBlockDimZ;
const int CHPL_GPU_ATTRIBUTE__MAX_GRID_DIM_X = hipDeviceAttributeMaxGridDimX;
const int CHPL_GPU_ATTRIBUTE__MAX_GRID_DIM_Y = hipDeviceAttributeMaxGridDimY;
const int CHPL_GPU_ATTRIBUTE__MAX_GRID_DIM_Z = hipDeviceAttributeMaxGridDimZ;
const int CHPL_GPU_ATTRIBUTE__MAX_SHARED_MEMORY_PER_BLOCK = hipDeviceAttributeMaxSharedMemoryPerBlock;
const int CHPL_GPU_ATTRIBUTE__TOTAL_CONSTANT_MEMORY = hipDeviceAttributeTotalConstantMemory;
const int CHPL_GPU_ATTRIBUTE__WARP_SIZE = hipDeviceAttributeWarpSize;
const int CHPL_GPU_ATTRIBUTE__MAX_PITCH = hipDeviceAttributeMaxPitch;
const int CHPL_GPU_ATTRIBUTE__MAXIMUM_TEXTURE1D_WIDTH = hipDeviceAttributeMaxTexture1DWidth;
const int CHPL_GPU_ATTRIBUTE__MAXIMUM_TEXTURE2D_WIDTH = hipDeviceAttributeMaxTexture2DWidth;
const int CHPL_GPU_ATTRIBUTE__MAXIMUM_TEXTURE2D_HEIGHT = hipDeviceAttributeMaxTexture2DHeight;
const int CHPL_GPU_ATTRIBUTE__MAXIMUM_TEXTURE3D_WIDTH = hipDeviceAttributeMaxTexture3DWidth;
const int CHPL_GPU_ATTRIBUTE__MAXIMUM_TEXTURE3D_HEIGHT = hipDeviceAttributeMaxTexture3DHeight;
const int CHPL_GPU_ATTRIBUTE__MAXIMUM_TEXTURE3D_DEPTH = hipDeviceAttributeMaxTexture3DDepth;
const int CHPL_GPU_ATTRIBUTE__MAX_REGISTERS_PER_BLOCK = hipDeviceAttributeMaxRegistersPerBlock;
const int CHPL_GPU_ATTRIBUTE__CLOCK_RATE = hipDeviceAttributeClockRate;
const int CHPL_GPU_ATTRIBUTE__TEXTURE_ALIGNMENT = hipDeviceAttributeTextureAlignment;
const int CHPL_GPU_ATTRIBUTE__TEXTURE_PITCH_ALIGNMENT = hipDeviceAttributeTexturePitchAlignment;
const int CHPL_GPU_ATTRIBUTE__MULTIPROCESSOR_COUNT = hipDeviceAttributeMultiprocessorCount;
const int CHPL_GPU_ATTRIBUTE__KERNEL_EXEC_TIMEOUT = hipDeviceAttributeKernelExecTimeout;
const int CHPL_GPU_ATTRIBUTE__INTEGRATED = hipDeviceAttributeIntegrated;
const int CHPL_GPU_ATTRIBUTE__CAN_MAP_HOST_MEMORY = hipDeviceAttributeCanMapHostMemory;
const int CHPL_GPU_ATTRIBUTE__COMPUTE_MODE = hipDeviceAttributeComputeMode;
const int CHPL_GPU_ATTRIBUTE__CONCURRENT_KERNELS = hipDeviceAttributeConcurrentKernels;
const int CHPL_GPU_ATTRIBUTE__ECC_ENABLED = hipDeviceAttributeEccEnabled;
const int CHPL_GPU_ATTRIBUTE__PCI_BUS_ID = hipDeviceAttributePciBusId;
const int CHPL_GPU_ATTRIBUTE__PCI_DEVICE_ID = hipDeviceAttributePciDeviceId;
const int CHPL_GPU_ATTRIBUTE__MEMORY_CLOCK_RATE = hipDeviceAttributeMemoryClockRate;
const int CHPL_GPU_ATTRIBUTE__GLOBAL_MEMORY_BUS_WIDTH = hipDeviceAttributeMemoryBusWidth;
const int CHPL_GPU_ATTRIBUTE__L2_CACHE_SIZE = hipDeviceAttributeL2CacheSize;
const int CHPL_GPU_ATTRIBUTE__MAX_THREADS_PER_MULTIPROCESSOR = hipDeviceAttributeMaxThreadsPerMultiProcessor;
const int CHPL_GPU_ATTRIBUTE__COMPUTE_CAPABILITY_MAJOR = hipDeviceAttributeComputeCapabilityMajor;
const int CHPL_GPU_ATTRIBUTE__COMPUTE_CAPABILITY_MINOR = hipDeviceAttributeComputeCapabilityMinor;
const int CHPL_GPU_ATTRIBUTE__MAX_SHARED_MEMORY_PER_MULTIPROCESSOR = hipDeviceAttributeMaxSharedMemoryPerMultiprocessor;
const int CHPL_GPU_ATTRIBUTE__MANAGED_MEMORY = hipDeviceAttributeManagedMemory;
const int CHPL_GPU_ATTRIBUTE__MULTI_GPU_BOARD = hipDeviceAttributeIsMultiGpuBoard;
const int CHPL_GPU_ATTRIBUTE__PAGEABLE_MEMORY_ACCESS = hipDeviceAttributePageableMemoryAccess;
const int CHPL_GPU_ATTRIBUTE__CONCURRENT_MANAGED_ACCESS = hipDeviceAttributeConcurrentManagedAccess;
const int CHPL_GPU_ATTRIBUTE__PAGEABLE_MEMORY_ACCESS_USES_HOST_PAGE_TABLES = hipDeviceAttributePageableMemoryAccessUsesHostPageTables;
const int CHPL_GPU_ATTRIBUTE__DIRECT_MANAGED_MEM_ACCESS_FROM_HOST = hipDeviceAttributeDirectManagedMemAccessFromHost;

int chpl_gpu_impl_query_attribute(int dev_lid, int attribute) {
  int res;
  int dev_pid = dev_lid_to_pid(dev_lid);
  ROCM_CALL(hipDeviceGetAttribute(&res, attribute, dev_pid));
  return res;
}

#endif // HAS_GPU_LOCALE
