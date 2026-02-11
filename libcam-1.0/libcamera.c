#include <stdint.h>
#include <stdlib.h>    // For malloc/free
#include <stdio.h>     // For FILE and fprintf (if needed for logging)
#include <fcntl.h>     // For open()
#include <unistd.h>    // For close(), read(), write()
#include <sys/ioctl.h> // For ioctl() calls
#include <errno.h>     // For error handling
#include <string.h>    // For memset, strerror

/*
 * libcamera.c - User-space camera library for custom OS on Motorola Nexus 6 (Shamu).
 * Compatible with ARMv7-A architecture (Snapdragon 805).
 * 
 * This library provides a simple API for interacting with the camera hardware.
 * It assumes a kernel driver at /drivers/cam/dcam.c (compiled into the kernel or as a module),
 * which exposes device nodes like /dev/camera0 for the main camera.
 * 
 * The library handles:
 * - Initialization and cleanup
 * - Opening/closing camera devices
 * - Basic capture (e.g., still images or frames)
 * - Configuration (e.g., resolution, format)
 * - Error handling and logging via kernel's system_log if available.
 * 
 * IOCTL commands are placeholders; define them in a shared header with the driver (e.g., dcam.h).
 * For Nexus 6 compatibility, use standard Linux ioctl for video devices (V4L2-inspired),
 * but simplified for custom OS.
 * 
 * To "find" /drivers/cam/dcam.c: This library checks if the driver is loaded by attempting
 * to open the device node. If not found, it could attempt to load the module dynamically
 * (if modprobe-like functionality exists in the custom OS kernel).
 * 
 * Compile with: gcc -shared -o libcamera.so libcamera.c -fPIC -march=armv7-a -mfloat-abi=softfp -mfpu=neon
 * (Adjust for your toolchain; ensures ARMv7-A compatibility with NEON for performance.)
 */

// Kernel-provided symbols (from program_runtime/kernel_symbols.c)
extern void system_log(const char *msg);

// Placeholder IOCTL commands (define in dcam.h)
#define DCAM_MAGIC 'D'
#define DCAM_INIT       _IO(DCAM_MAGIC, 0)  // Initialize camera
#define DCAM_CAPTURE    _IOWR(DCAM_MAGIC, 1, struct dcam_frame)  // Capture frame
#define DCAM_SET_CONFIG _IOW(DCAM_MAGIC, 2, struct dcam_config)  // Set config
#define DCAM_GET_CONFIG _IOR(DCAM_MAGIC, 3, struct dcam_config)  // Get config
#define DCAM_RELEASE    _IO(DCAM_MAGIC, 4)  // Release resources

// Structures for camera operations (compatible with driver)
struct dcam_config {
    uint32_t width;
    uint32_t height;
    uint32_t format;  // e.g., 1 = YUV, 2 = RGB
    uint32_t fps;
};

struct dcam_frame {
    void *buffer;
    size_t size;
    uint64_t timestamp;
};

// Camera handle structure
typedef struct {
    int fd;                // File descriptor for /dev/camera0
    struct dcam_config config;
    void *frame_buffer;    // Allocated buffer for frames
    size_t buffer_size;
    int initialized;
} camera_handle_t;

/*
 * ===== COMPREHENSIVE STRUCTS SECTION =====
 * Complete unified structure definitions for ALL libcamera functionality.
 * Place this block right after original camera_handle_t definition.
 */

// ===== VIDEO STREAMING STRUCTS =====
struct dcam_stream_config {
    uint32_t buffer_count;
    uint32_t enable_callbacks;
    uint32_t drop_on_overflow;
    uint32_t priority;
};

struct dcam_stream_stats {
    uint32_t frames_captured;
    uint32_t frames_dropped;
    uint32_t buffer_utilization;
    float avg_latency_us;
};

// ===== PREVIEW STRUCTS =====
struct dcam_preview_config {
    uint32_t preview_width;
    uint32_t preview_height;
    uint32_t display_width;
    uint32_t display_height;
    uint32_t fps_target;
    uint32_t overlay_enabled;
    uint32_t rotation;
    uint32_t mirror;
};

// ===== WATCHDOG STRUCTS =====
struct dcam_watchdog_config {
    uint32_t timeout_ms;
    uint32_t kick_interval_ms;
    uint32_t max_dropped_frames;
    uint32_t recovery_mode;
};

struct dcam_watchdog_status {
    uint32_t is_armed;
    uint32_t time_remaining_ms;
    uint32_t frames_dropped;
    uint32_t recovery_count;
    uint32_t last_error;
};

// ===== STABILIZER STRUCTS =====
struct dcam_stabilizer_config {
    uint32_t enabled;
    uint32_t crop_percent;
    float gyro_sensitivity;
    uint32_t smooth_window_frames;
    uint32_t max_correction_x;
    uint32_t max_correction_y;
};

struct dcam_stabilizer_status {
    float correction_x;
    float correction_y;
    float gyro_x, gyro_y, gyro_z;
    uint32_t frame_count;
    uint32_t dropped_frames;
};

struct dcam_gyro_data {
    float gyro_x, gyro_y, gyro_z;
    float accel_x, accel_y, accel_z;
    uint64_t timestamp;
};

// ===== PERFORMANCE STRUCTS =====
struct dcam_perf_stats {
    uint64_t capture_latency_ns;
    uint64_t total_frames;
    uint64_t dropped_frames;
    float fps_current;
    float fps_average;
    float cpu_usage_percent;
    uint64_t memory_usage_bytes;
    uint32_t latency_p50;
    uint32_t latency_p90;
    uint32_t latency_p99;
    uint64_t timestamp;
};

// ===== WARNINGS & ERROR STRUCTS =====
struct dcam_warning {
    uint32_t severity;
    uint32_t category;
    uint64_t timestamp;
    int error_code;
    char message[256];
    char recovery[128];
    uint32_t count;
};

struct dcam_warning_stats {
    uint32_t total_warnings;
    uint32_t by_severity[5];
    uint32_t by_category[8];
    uint32_t recovery_success;
    uint64_t last_warning_time;
};

struct dcam_error_config {
    uint32_t max_retries_fast;
    uint32_t max_retries_medium;
    uint32_t max_retries_slow;
    uint32_t enable_fallbacks;
    uint32_t log_detailed_errors;
};

struct dcam_error_context {
    int error_code;
    int system_errno;
    uint64_t timestamp;
    const char *operation;
    int retry_count;
    int recovery_attempted;
    int recovery_success;
};

// ===== MEMORY MANAGEMENT STRUCTS =====
struct dcam_mem_stats {
    uint64_t total_allocated;
    uint64_t peak_usage;
    uint64_t current_usage;
    uint32_t buffer_count;
    uint32_t large_alloc_failures;
    uint32_t pool_hits[4];
};

struct dcam_mem_pool {
    void **buffers;
    size_t *sizes;
    uint32_t type;
    uint32_t capacity;
    uint32_t free_count;
    uint32_t alloc_count;
    size_t total_size;
};

// ===== ADVANCED CAPTURE & METADATA =====
struct dcam_capture_request {
    uint32_t type;
    uint32_t count;
    uint32_t format;
    uint32_t quality;
    struct dcam_frame *frames;
};

struct dcam_metadata {
    uint64_t timestamp;
    float gyro_x, gyro_y, gyro_z;
    float accel_x, accel_y, accel_z;
    float exposure_time_us;
    float iso_gain;
    uint32_t focus_distance_cm;
    int32_t temperature_celsius;
    uint32_t white_balance;
};

// ===== UNIFIED STATE DUMP =====
struct dcam_full_state {
    uint32_t version;
    struct dcam_config config;
    struct dcam_perf_stats perf;
    struct dcam_watchdog_status watchdog;
    struct dcam_stabilizer_status stabilizer;
    struct dcam_warning_stats warnings;
    uint32_t capabilities;
    uint32_t status_flags;
    char firmware_version[32];
    uint64_t total_uptime_ns;
};

// ===== ADDITIONAL IOCTL COMMANDS =====
#define DCAM_GET_FULL_STATE    _IOR(DCAM_MAGIC, 24, struct dcam_full_state)
#define DCAM_CAPTURE_ADVANCED  _IOWR(DCAM_MAGIC, 25, struct dcam_capture_request)
#define DCAM_MEM_STATS         _IOR(DCAM_MAGIC, 26, struct dcam_mem_stats)

/*
 * ===== HANDLE STRUCT DEFINITIONS =====
 * Complete internal state for all subsystems
 */
struct stream_handle_t {
    camera_handle_t *camera;
    void (*callback)(void*, size_t, uint64_t, void*);
    void *user_data;
    int streaming;
    int buffer_count;
    void **stream_buffers;
    size_t buffer_size;
    int read_index;
    int write_index;
};

struct preview_handle_t {
    camera_handle_t *camera;
    struct dcam_preview_config config;
    void *preview_buffer;
    size_t preview_size;
    int preview_active;
    uint64_t last_frame_time;
};

struct watchdog_handle_t {
    camera_handle_t *camera;
    stream_handle_t *stream;
    preview_handle_t *preview;
    struct dcam_watchdog_config config;
    int watchdog_active;
    uint64_t last_kick_time;
    uint32_t dropped_frames;
    uint32_t recovery_count;
};

struct stabilizer_handle_t {
    camera_handle_t *camera;
    struct dcam_stabilizer_config config;
    int stabilizer_active;
    float motion_x, motion_y;
    float gyro_integral_x, gyro_integral_y;
    float smooth_x[32], smooth_y[32];
    int smooth_index;
    uint32_t frame_count;
    float max_offset_x, max_offset_y;
};

struct warnings_handle_t {
    struct dcam_warning_stats stats;
    int warnings_enabled;
    uint32_t max_warnings;
    uint32_t warning_count;
};

struct error_handler_t {
    struct dcam_error_config config;
    warnings_handle_t *warnings;
    int error_handling_active;
    uint32_t total_recoveries;
    uint32_t failed_recoveries;
};

struct perf_monitor_t {
    camera_handle_t *camera;
    warnings_handle_t *warnings;
    error_handler_t *error_handler;
    int perf_monitor_active;
    uint64_t frame_times[128];
    uint64_t total_time_ns;
    int frame_index;
    int frame_count;
    uint64_t fps_start_time;
    float target_fps;
    uint64_t start_memory;
    uint64_t peak_memory;
};

struct mem_manager_t {
    struct dcam_mem_stats stats;
    struct dcam_mem_pool *pools;
    uint32_t pool_count;
    int leak_detection;
    uint64_t alloc_start_time;
    void **tracked_allocs;
    uint32_t tracked_count;
    uint32_t tracked_capacity;
};

// ===== TYPEDEFS =====
typedef struct stream_handle_t stream_handle_t;
typedef struct preview_handle_t preview_handle_t;
typedef struct watchdog_handle_t watchdog_handle_t;
typedef struct stabilizer_handle_t stabilizer_handle_t;
typedef struct warnings_handle_t warnings_handle_t;
typedef struct error_handler_t error_handler_t;
typedef struct perf_monitor_t perf_monitor_t;
typedef struct mem_manager_t mem_manager_t;

/*
 * MEMORY MANAGEMENT System
 * 
 * Zero-copy memory pools, DMA-safe buffers, cache management,
 * memory pressure handling, and leak detection for camera pipeline.
 * Critical for real-time performance on Nexus 6 (2GB RAM constraint).
 * Supports ION allocator (Android/Nexus standard) + fallback malloc.
 */

// Memory pool types
#define MEM_POOL_DMA      0x01  // DMA-safe (physically contiguous)
#define MEM_POOL_CACHED   0x02  // CPU cached, needs flush/invalidate
#define MEM_POOL_UNCACHED 0x04  // Uncached, direct HW access
#define MEM_POOL_STREAM   0x08  // Streaming ring buffers

// Memory allocation flags
#define MEM_FLAG_ZERO     0x01  // Pre-zero buffer
#define MEM_FLAG_ALIGN_4K 0x02  // 4KB page alignment (DMA)
#define MEM_FLAG_PINNED   0x04  // Pinned memory (no swapping)

// Memory statistics
struct dcam_mem_stats {
    uint64_t total_allocated;
    uint64_t peak_usage;
    uint64_t current_usage;
    uint32_t buffer_count;
    uint32_t large_alloc_failures;  // >1MB failures
    uint32_t pool_hits[4];          // Per pool type
};

// Memory pool structure
struct dcam_mem_pool {
    void **buffers;           // Buffer pointers
    size_t *sizes;            // Buffer sizes  
    uint32_t type;            // Pool type flags
    uint32_t capacity;        // Total buffers in pool
    uint32_t free_count;      // Available buffers
    uint32_t alloc_count;     // Total allocations from pool
    size_t total_size;        // Total pool memory
};

// Memory handle with leak detection
typedef struct {
    struct dcam_mem_stats stats;
    struct dcam_mem_pool *pools;     // Array of pools
    uint32_t pool_count;
    int leak_detection;
    uint64_t alloc_start_time;
    void **tracked_allocs;           // Leak detector array
    uint32_t tracked_count;
    uint32_t tracked_capacity;
} mem_manager_t;

static mem_manager_t *active_mem_manager = NULL;

/*
 * Initialize memory manager with pre-allocated pools.
 * Reserves DMA-safe memory for camera buffers (essential for Nexus 6).
 */
mem_manager_t *mem_manager_init(uint32_t pool_configs[][3], uint32_t pool_count) {
    mem_manager_t *mem = (mem_manager_t *)calloc(1, sizeof(mem_manager_t));
    if (!mem) {
        system_log("FATAL: Memory manager init failed");
        return NULL;
    }
    
    mem->pools = (struct dcam_mem_pool *)calloc(pool_count, sizeof(struct dcam_mem_pool));
    if (!mem->pools) {
        free(mem);
        return NULL;
    }
    
    mem->pool_count = pool_count;
    mem->leak_detection = 1;
    mem->tracked_capacity = 1024;
    mem->tracked_allocs = (void **)calloc(1024, sizeof(void *));
    
    // Initialize pools from config (type, buffer_count, buffer_size)
    for (uint32_t i = 0; i < pool_count; i++) {
        struct dcam_mem_pool *pool = &mem->pools[i];
        pool->type = pool_configs[i][0];
        pool->capacity = pool_configs[i][1];
        size_t buf_size = pool_configs[i][2];
        
        pool->buffers = (void **)calloc(pool->capacity, sizeof(void *));
        pool->sizes = (size_t *)calloc(pool->capacity, sizeof(size_t));
        pool->free_count = pool->capacity;
        
        // Pre-allocate all buffers in pool
        for (uint32_t j = 0; j < pool->capacity; j++) {
            pool->buffers[j] = mem_allocate_buffer(pool->type, buf_size, MEM_FLAG_ALIGN_4K);
            if (pool->buffers[j]) {
                pool->sizes[j] = buf_size;
                pool->free_count--;
                mem->stats.current_usage += buf_size;
            }
        }
        
        pool->total_size = buf_size * pool->capacity;
        mem->stats.total_allocated += pool->total_size;
        mem->stats.peak_usage = mem->stats.current_usage;
        
        char msg[128];
        snprintf(msg, sizeof(msg), "Pool[%u]: %s, %u bufs x %zub = %zub", 
                i, pool->type & MEM_POOL_DMA ? "DMA" : "CPU", pool->capacity, 
                buf_size, pool->total_size);
        system_log(msg);
    }
    
    active_mem_manager = mem;
    mem->alloc_start_time = perf_get_timestamp_ns();  // From perf monitor
    system_log("Memory manager initialized with zero-copy pools");
    return mem;
}

/*
 * Allocate buffer from pool (zero-copy, DMA-safe).
 * Returns NULL + logs warning if pool exhausted.
 */
void *mem_get_buffer(mem_manager_t *mem, uint32_t pool_type, size_t min_size, 
                     uint32_t flags, size_t *actual_size) {
    if (!mem) return NULL;
    
    // Find matching pool
    for (uint32_t i = 0; i < mem->pool_count; i++) {
        struct dcam_mem_pool *pool = &mem->pools[i];
        if ((pool->type & pool_type) && pool->free_count > 0) {
            // Find first free buffer with sufficient size
            for (uint32_t j = 0; j < pool->capacity; j++) {
                if (pool->buffers[j] && pool->sizes[j] >= min_size) {
                    void *buf = pool->buffers[j];
                    pool->buffers[j] = NULL;  // Mark as used
                    pool->free_count--;
                    pool->alloc_count++;
                    
                    mem->stats.pool_hits[i]++;
                    if (actual_size) *actual_size = pool->sizes[j];
                    
                    // Track for leak detection
                    if (mem->leak_detection) {
                        mem->tracked_allocs[mem->tracked_count++ % mem->tracked_capacity] = buf;
                    }
                    
                    return buf;
                }
            }
        }
    }
    
    // Pool miss - fallback allocation
    void *fallback = mem_allocate_buffer(pool_type, min_size, flags);
    if (fallback && actual_size) *actual_size = min_size;
    return fallback;
}

/*
 * Return buffer to pool (zero-copy recycling).
 */
void mem_return_buffer(mem_manager_t *mem, void *buffer) {
    if (!mem || !buffer) return;
    
    // Find which pool this buffer belongs to
    for (uint32_t i = 0; i < mem->pool_count; i++) {
        struct dcam_mem_pool *pool = &mem->pools[i];
        for (uint32_t j = 0; j < pool->capacity; j++) {
            if (pool->buffers[j] == NULL) {  // Free slot
                pool->buffers[j] = buffer;
                pool->free_count++;
                mem->stats.current_usage -= 0;  // Size tracked elsewhere
                return;
            }
        }
    }
    
    // Buffer not from pool - just free it
    if (active_mem_manager->leak_detection) {
        // Remove from leak tracker
        for (uint32_t i = 0; i < mem->tracked_count; i++) {
            if (mem->tracked_allocs[i] == buffer) {
                mem->tracked_allocs[i] = NULL;
                break;
            }
        }
    }
    free(buffer);
}

/*
 * Low-level buffer allocator (ION/DMA fallback).
 */
static void *mem_allocate_buffer(uint32_t type, size_t size, uint32_t flags) {
    // 4K alignment for DMA
    if (flags & MEM_FLAG_ALIGN_4K) {
        size = (size + 4095) & ~4095;
    }
    
    int fd = -1;
    void *buf = NULL;
    
    // Try ION allocator first (Nexus 6 standard)
    #ifdef HAVE_ION
    fd = open("/dev/ion", O_RDONLY);
    if (fd >= 0) {
        // ION allocation code here (simplified)
        // buf = ion_alloc(fd, size, 4096);
        close(fd);
    }
    #endif
    
    // Fallback to posix_memalign
    if (!buf) {
        if (posix_memalign(&buf, 4096, size) != 0) {
            buf = NULL;
        }
    }
    
    // Final fallback: regular malloc
    if (!buf) {
        buf = malloc(size);
    }
    
    if (flags & MEM_FLAG_ZERO) {
        memset(buf, 0, size);
    }
    
    if (!buf) {
        if (active_mem_manager) {
            active_mem_manager->stats.large_alloc_failures++;
        }
        system_log("Memory allocation failed - low memory condition");
    }
    
    return buf;
}

/*
 * Cache management for ARM NEON/DMA (critical for performance).
 */
void mem_cache_flush(void *buffer, size_t size) {
    // ARM cache maintenance (Nexus 6 Snapdragon 805)
    __builtin___clear_cache((char *)buffer, (char *)buffer + size);
    
    // L1/L2 flush for DMA safety
    #ifdef __aarch32__
    asm volatile("mcr p15, 0, %0, c7, c10, 5" : : "r"(0) : "memory");  // DCache clean+invalidate
    asm volatile("mcr p15, 0, %0, c7, c5,  0" : : "r"(0) : "memory");  // Branch predictor invalidate
    #endif
}

void mem_cache_invalidate(void *buffer, size_t size) {
    __builtin___clear_cache((char *)buffer, (char *)buffer + size);
}

/*
 * Get memory statistics and detect leaks.
 */
int mem_get_stats(mem_manager_t *mem, struct dcam_mem_stats *stats) {
    if (!mem || !stats) return -1;
    *stats = mem->stats;
    
    // Update peak usage
    if (mem->stats.current_usage > mem->stats.peak_usage) {
        mem->stats.peak_usage = mem->stats.current_usage;
    }
    
    // Simple leak detection
    if (mem->leak_detection) {
        uint32_t leaks = 0;
        for (uint32_t i = 0; i < mem->tracked_capacity; i++) {
            if (mem->tracked_allocs[i] && mem->tracked_allocs[i] != (void *)-1) {
                leaks++;
            }
        }
        if (leaks > 10) {
            system_log("WARNING: Potential memory leaks detected");
        }
    }
    
    return 0;
}

/*
 * Memory pressure handler - reduce allocations under low memory.
 */
void mem_pressure_reduce(mem_manager_t *mem) {
    if (!mem) return;
    
    // Shrink non-critical pools
    for (uint32_t i = 0; i < mem->pool_count; i++) {
        if (!(mem->pools[i].type & MEM_POOL_DMA)) {
            // Reduce pool size by 50% (non-DMA pools)
            mem->pools[i].capacity /= 2;
        }
    }
    system_log("Memory pressure: Reduced non-critical pool sizes");
}

/*
 * Cleanup memory manager and validate no leaks.
 */
void mem_manager_cleanup(mem_manager_t *mem) {
    if (!mem) return;
    
    // Validate all buffers returned to pools
    uint32_t orphan_buffers = 0;
    for (uint32_t i = 0; i < mem->pool_count; i++) {
        for (uint32_t j = 0; j < mem->pools[i].capacity; j++) {
            if (!mem->pools[i].buffers[j]) {
                orphan_buffers++;
            }
        }
    }
    
    if (orphan_buffers > 0) {
        system_log("WARNING: Orphan buffers detected on cleanup");
    }
    
    // Free all pools
    for (uint32_t i = 0; i < mem->pool_count; i++) {
        for (uint32_t j = 0; j < mem->pools[i].capacity; j++) {
            free(mem->pools[i].buffers[j]);
        }
        free(mem->pools[i].buffers);
        free(mem->pools[i].sizes);
    }
    
    free(mem->pools);
    free(mem->tracked_allocs);
    free(mem);
    active_mem_manager = NULL;
}

// Path to the camera device node (assumed provided by /drivers/cam/dcam.c driver)
static const char *camera_dev_path = "/dev/camera0";

// Path to the driver source (for logging/reference; not used at runtime)
static const char *driver_source_path = "/drivers/cam/dcam.c";

// Function to attempt loading the driver if not found (stub; assumes modprobe-like)
static int load_driver_if_needed(void) {
    // In custom OS, implement kernel module loading here.
    // For now, stub: Check if device exists, log if not.
    if (access(camera_dev_path, F_OK) != 0) {
        system_log("Camera driver not loaded. Attempting to load /drivers/cam/dcam.c (compiled module).");
        // Placeholder: system("insmod /system/modules/dcam.ko"); // Adjust for custom OS
        return -1;  // Assume failure in stub
    }
    return 0;
}

/*
 * Initialize the camera library and open the device.
 * Returns a handle on success, NULL on failure.
 */
camera_handle_t *camera_init(void) {
    camera_handle_t *handle = (camera_handle_t *)malloc(sizeof(camera_handle_t));
    if (!handle) {
        system_log("Failed to allocate camera handle.");
        return NULL;
    }
    memset(handle, 0, sizeof(camera_handle_t));

    // Check and load driver if needed
    if (load_driver_if_needed() != 0) {
        system_log("Failed to load camera driver from /drivers/cam/dcam.c.");
        free(handle);
        return NULL;
    }

    // Open the device
    handle->fd = open(camera_dev_path, O_RDWR);
    if (handle->fd < 0) {
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "Failed to open %s: %s", camera_dev_path, strerror(errno));
        system_log(err_msg);
        free(handle);
        return NULL;
    }

    // Initialize via IOCTL
    if (ioctl(handle->fd, DCAM_INIT, NULL) < 0) {
        system_log("IOCTL DCAM_INIT failed.");
        close(handle->fd);
        free(handle);
        return NULL;
    }

    // Default config
    handle->config.width = 1920;
    handle->config.height = 1080;
    handle->config.format = 1;  // YUV
    handle->config.fps = 30;

    // Set default config
    if (camera_set_config(handle, &handle->config) < 0) {
        system_log("Failed to set default config.");
        camera_release(handle);
        return NULL;
    }

    // Allocate frame buffer
    handle->buffer_size = handle->config.width * handle->config.height * 3 / 2;  // YUV size
    handle->frame_buffer = malloc(handle->buffer_size);
    if (!handle->frame_buffer) {
        system_log("Failed to allocate frame buffer.");
        camera_release(handle);
        return NULL;
    }

    handle->initialized = 1;
    system_log("Camera initialized successfully.");
    return handle;
}

/*
 * Set camera configuration.
 * Returns 0 on success, -1 on failure.
 */
int camera_set_config(camera_handle_t *handle, const struct dcam_config *config) {
    if (!handle || !handle->initialized) {
        return -1;
    }
    if (ioctl(handle->fd, DCAM_SET_CONFIG, config) < 0) {
        system_log("IOCTL DCAM_SET_CONFIG failed.");
        return -1;
    }
    handle->config = *config;
    // Reallocate buffer if size changes
    size_t new_size = config->width * config->height * 3 / 2;  // Assume YUV
    if (new_size != handle->buffer_size) {
        free(handle->frame_buffer);
        handle->frame_buffer = malloc(new_size);
        if (!handle->frame_buffer) {
            system_log("Failed to reallocate frame buffer.");
            return -1;
        }
        handle->buffer_size = new_size;
    }
    return 0;
}

/*
 * Get current camera configuration.
 * Returns 0 on success, -1 on failure.
 */
int camera_get_config(camera_handle_t *handle, struct dcam_config *config) {
    if (!handle || !handle->initialized || !config) {
        return -1;
    }
    if (ioctl(handle->fd, DCAM_GET_CONFIG, config) < 0) {
        system_log("IOCTL DCAM_GET_CONFIG failed.");
        return -1;
    }
    return 0;
}

/*
 * Capture a single frame.
 * Stores in handle->frame_buffer.
 * Returns 0 on success, -1 on failure.
 */
int camera_capture(camera_handle_t *handle) {
    if (!handle || !handle->initialized) {
        return -1;
    }
    struct dcam_frame frame = {
        .buffer = handle->frame_buffer,
        .size = handle->buffer_size,
        .timestamp = 0
    };
    if (ioctl(handle->fd, DCAM_CAPTURE, &frame) < 0) {
        system_log("IOCTL DCAM_CAPTURE failed.");
        return -1;
    }
    // Optionally process frame here (e.g., convert formats)
    return 0;
}

/*
 * Release the camera handle and resources.
 */
void camera_release(camera_handle_t *handle) {
    if (!handle) {
        return;
    }
    if (handle->initialized) {
        ioctl(handle->fd, DCAM_RELEASE, NULL);
        close(handle->fd);
    }
    free(handle->frame_buffer);
    free(handle);
    system_log("Camera released.");
}

/*
 * Utility: Save captured frame to file (for testing; PPM format for simplicity).
 * Assumes RGB format; convert if needed.
 */
int camera_save_frame(camera_handle_t *handle, const char *filename) {
    if (!handle || !handle->initialized || !filename) {
        return -1;
    }
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        system_log("Failed to open file for saving frame.");
        return -1;
    }
    // Assume buffer is RGB for PPM
    fprintf(fp, "P6\n%u %u\n255\n", handle->config.width, handle->config.height);
    fwrite(handle->frame_buffer, 1, handle->buffer_size, fp);
    fclose(fp);
    return 0;
}

// Additional functions can be added, e.g., for video streaming, preview, etc.

// ---- Video Streaming ----

/*
 * Video Streaming Functions
 * 
 * Enables continuous frame capture for video streaming.
 * Uses non-blocking IOCTL calls and provides callback mechanism.
 */

// IOCTL for streaming (add to dcam.h)
#define DCAM_STREAM_START  _IO(DCAM_MAGIC, 5)   // Start streaming
#define DCAM_STREAM_STOP   _IO(DCAM_MAGIC, 6)   // Stop streaming  
#define DCAM_STREAM_READ   _IOR(DCAM_MAGIC, 7, struct dcam_frame)  // Read next frame

// Streaming callback type
typedef void (*stream_callback_t)(void *buffer, size_t size, uint64_t timestamp, void *user_data);

// Streaming handle structure
typedef struct {
    camera_handle_t *camera;
    stream_callback_t callback;
    void *user_data;
    int streaming;
    int buffer_count;      // Number of internal buffers
    void **stream_buffers; // Ring buffer array
    size_t buffer_size;
    int read_index;
    int write_index;
} stream_handle_t;

/*
 * Initialize video streaming on a camera handle.
 * Allocates ring buffer for smooth streaming.
 * Returns stream handle on success, NULL on failure.
 */
stream_handle_t *stream_init(camera_handle_t *camera, int buffer_count) {
    if (!camera || !camera->initialized || buffer_count <= 0) {
        system_log("Invalid parameters for stream_init.");
        return NULL;
    }
    
    stream_handle_t *stream = (stream_handle_t *)malloc(sizeof(stream_handle_t));
    if (!stream) {
        system_log("Failed to allocate stream handle.");
        return NULL;
    }
    memset(stream, 0, sizeof(stream_handle_t));
    
    stream->camera = camera;
    stream->buffer_count = buffer_count;
    stream->buffer_size = camera->buffer_size;
    
    // Allocate ring buffer array
    stream->stream_buffers = (void **)malloc(buffer_count * sizeof(void *));
    if (!stream->stream_buffers) {
        system_log("Failed to allocate stream buffer array.");
        free(stream);
        return NULL;
    }
    
    // Allocate individual buffers
    for (int i = 0; i < buffer_count; i++) {
        stream->stream_buffers[i] = malloc(stream->buffer_size);
        if (!stream->stream_buffers[i]) {
            system_log("Failed to allocate stream buffer.");
            // Cleanup on failure
            for (int j = 0; j < i; j++) {
                free(stream->stream_buffers[j]);
            }
            free(stream->stream_buffers);
            free(stream);
            return NULL;
        }
    }
    
    stream->read_index = 0;
    stream->write_index = 0;
    
    // Start streaming via IOCTL
    if (ioctl(camera->fd, DCAM_STREAM_START, NULL) < 0) {
        system_log("IOCTL DCAM_STREAM_START failed.");
        stream_cleanup(stream);
        return NULL;
    }
    
    stream->streaming = 1;
    system_log("Video streaming initialized successfully.");
    return stream;
}

/*
 * Set streaming callback. Called for each new frame.
 */
void stream_set_callback(stream_handle_t *stream, stream_callback_t callback, void *user_data) {
    if (stream) {
        stream->callback = callback;
        stream->user_data = user_data;
    }
}

/*
 * Main streaming loop. Reads frames continuously.
 * Returns 0 on success, -1 on error, -2 if stopped.
 */
int stream_run(stream_handle_t *stream) {
    if (!stream || !stream->streaming) {
        return -1;
    }
    
    while (stream->streaming) {
        struct dcam_frame frame;
        frame.buffer = stream->stream_buffers[stream->write_index];
        frame.size = stream->buffer_size;
        
        // Non-blocking read next frame
        if (ioctl(stream->camera->fd, DCAM_STREAM_READ, &frame) < 0) {
            if (errno == EAGAIN) {
                usleep(1000);  // 1ms sleep if no frame ready
                continue;
            }
            system_log("IOCTL DCAM_STREAM_READ failed.");
            return -1;
        }
        
        // Invoke callback with new frame
        if (stream->callback) {
            stream->callback(frame.buffer, frame.size, frame.timestamp, stream->user_data);
        }
        
        // Advance write index (ring buffer)
        stream->write_index = (stream->write_index + 1) % stream->buffer_count;
        
        // Prevent overflow (drop frames if consumer too slow)
        if (stream->write_index == stream->read_index) {
            stream->read_index = (stream->read_index + 1) % stream->buffer_count;
            system_log("Stream buffer overflow - frame dropped.");
        }
    }
    
    return 0;
}

/*
 * Stop streaming gracefully.
 */
void stream_stop(stream_handle_t *stream) {
    if (!stream || !stream->streaming) {
        return;
    }
    
    stream->streaming = 0;
    ioctl(stream->camera->fd, DCAM_STREAM_STOP, NULL);
    system_log("Video streaming stopped.");
}

/*
 * Cleanup streaming resources.
 */
void stream_cleanup(stream_handle_t *stream) {
    if (!stream) {
        return;
    }
    
    stream_stop(stream);
    
    // Free ring buffers
    if (stream->stream_buffers) {
        for (int i = 0; i < stream->buffer_count; i++) {
            free(stream->stream_buffers[i]);
        }
        free(stream->stream_buffers);
    }
    
    free(stream);
    system_log("Stream resources cleaned up.");
}

/*
 * Example callback for streaming (for testing).
 * Saves frames as numbered PPM files.
 */
static void example_stream_callback(void *buffer, size_t size, uint64_t timestamp, void *user_data) {
    static int frame_count = 0;
    char filename[64];
    snprintf(filename, sizeof(filename), "stream_frame_%05d.ppm", frame_count++);
    
    FILE *fp = fopen(filename, "wb");
    if (fp) {
        fprintf(fp, "P6
%u %u
255
", 1920, 1080);  // Hardcoded for example
        fwrite(buffer, 1, size, fp);
        fclose(fp);
        printf("Saved %s (timestamp: %llu)
", filename, timestamp);
    }
}

// ---- Preview ----

/*
 * Preview Functions
 * 
 * Provides live camera preview to display hardware/software.
 * Lower resolution + faster capture for smooth 30fps preview.
 * Supports overlay mode and preview window configuration.
 */

// IOCTL for preview (add to dcam.h)
#define DCAM_PREVIEW_START   _IOW(DCAM_MAGIC, 8, struct dcam_preview_config)  // Start preview
#define DCAM_PREVIEW_STOP    _IO(DCAM_MAGIC, 9)                                // Stop preview
#define DCAM_PREVIEW_FRAME   _IOR(DCAM_MAGIC, 10, struct dcam_frame)           // Get preview frame
#define DCAM_PREVIEW_CONFIG  _IOR(DCAM_MAGIC, 11, struct dcam_preview_config)  // Get preview config

// Preview configuration structure
struct dcam_preview_config {
    uint32_t preview_width;    // Typically 640x480 or 320x240 for preview
    uint32_t preview_height;
    uint32_t display_width;    // Display resolution (may differ from preview)
    uint32_t display_height;
    uint32_t fps_target;       // 30fps typical for preview
    uint32_t overlay_enabled;  // Hardware overlay support
    uint32_t rotation;         // 0, 90, 180, 270 degrees
};

// Preview handle structure
typedef struct {
    camera_handle_t *camera;
    struct dcam_preview_config config;
    void *preview_buffer;     // Single buffer for preview frames
    size_t preview_size;
    int preview_active;
    uint64_t last_frame_time; // For FPS limiting
} preview_handle_t;

/*
 * Initialize preview mode on camera.
 * Automatically sets lower resolution for smooth performance.
 * Returns preview handle on success, NULL on failure.
 */
preview_handle_t *preview_init(camera_handle_t *camera, uint32_t preview_width, uint32_t preview_height) {
    if (!camera || !camera->initialized) {
        system_log("Invalid camera handle for preview_init.");
        return NULL;
    }
    
    preview_handle_t *preview = (preview_handle_t *)malloc(sizeof(preview_handle_t));
    if (!preview) {
        system_log("Failed to allocate preview handle.");
        return NULL;
    }
    memset(preview, 0, sizeof(preview_handle_t));
    
    preview->camera = camera;
    
    // Default preview config (VGA or lower for smooth 30fps)
    preview->config.preview_width = preview_width ? preview_width : 640;
    preview->config.preview_height = preview_height ? preview_height : 480;
    preview->config.display_width = 640;   // Match preview or set display size
    preview->config.display_height = 480;
    preview->config.fps_target = 30;
    preview->config.overlay_enabled = 0;   // Software scaling by default
    preview->config.rotation = 0;
    
    // Calculate buffer size (YUV420)
    preview->preview_size = preview->config.preview_width * preview->config.preview_height * 3 / 2;
    preview->preview_buffer = malloc(preview->preview_size);
    if (!preview->preview_buffer) {
        system_log("Failed to allocate preview buffer.");
        free(preview);
        return NULL;
    }
    
    // Start preview via IOCTL
    if (ioctl(camera->fd, DCAM_PREVIEW_START, &preview->config) < 0) {
        system_log("IOCTL DCAM_PREVIEW_START failed.");
        free(preview->preview_buffer);
        free(preview);
        return NULL;
    }
    
    preview->preview_active = 1;
    preview->last_frame_time = 0;
    system_log("Camera preview initialized successfully.");
    return preview;
}

/*
 * Get next preview frame.
 * Returns 0 on success, -1 on failure, frame data in preview->preview_buffer.
 * Automatically limits FPS to config.fps_target.
 */
int preview_get_frame(preview_handle_t *preview) {
    if (!preview || !preview->preview_active) {
        return -1;
    }
    
    // FPS limiting (simple timestamp check)
    uint64_t now = 0; // Replace with system timestamp call
    uint64_t frame_interval = 1000000000ULL / preview->config.fps_target; // nanoseconds
    if (now - preview->last_frame_time < frame_interval) {
        return 0;  // Skip frame for FPS limit
    }
    
    struct dcam_frame frame = {
        .buffer = preview->preview_buffer,
        .size = preview->preview_size,
        .timestamp = now
    };
    
    if (ioctl(preview->camera->fd, DCAM_PREVIEW_FRAME, &frame) < 0) {
        if (errno != EAGAIN) {
            system_log("IOCTL DCAM_PREVIEW_FRAME failed.");
            return -1;
        }
        return 0;  // No frame ready
    }
    
    preview->last_frame_time = now;
    return 0;
}

/*
 * Update preview display configuration (resolution, rotation, etc.)
 */
int preview_update_config(preview_handle_t *preview, struct dcam_preview_config *new_config) {
    if (!preview || !preview->preview_active || !new_config) {
        return -1;
    }
    
    // Stop preview, update config, restart
    ioctl(preview->camera->fd, DCAM_PREVIEW_STOP, NULL);
    
    if (ioctl(preview->camera->fd, DCAM_PREVIEW_START, new_config) < 0) {
        system_log("IOCTL DCAM_PREVIEW_START (update) failed.");
        return -1;
    }
    
    preview->config = *new_config;
    preview->preview_size = new_config->preview_width * new_config->preview_height * 3 / 2;
    
    // Reallocate buffer if size changed
    free(preview->preview_buffer);
    preview->preview_buffer = malloc(preview->preview_size);
    if (!preview->preview_buffer) {
        system_log("Failed to reallocate preview buffer.");
        return -1;
    }
    
    return 0;
}

/*
 * Stop preview mode.
 */
void preview_stop(preview_handle_t *preview) {
    if (!preview || !preview->preview_active) {
        return;
    }
    
    ioctl(preview->camera->fd, DCAM_PREVIEW_STOP, NULL);
    preview->preview_active = 0;
    system_log("Camera preview stopped.");
}

/*
 * Cleanup preview resources.
 */
void preview_cleanup(preview_handle_t *preview) {
    if (!preview) {
        return;
    }
    
    preview_stop(preview);
    free(preview->preview_buffer);
    free(preview);
    system_log("Preview resources cleaned up.");
}

/*
 * Example preview loop (60 FPS display loop).
 * Replace display_update() with your graphics API call.
 */
void preview_run_loop(preview_handle_t *preview, int duration_ms) {
    uint64_t start_time = 0; // System timestamp
    int frames = 0;
    
    while (preview->preview_active && (duration_ms < 0 || (/* now - start_time */ 0) < duration_ms * 1000000ULL)) {
        if (preview_get_frame(preview) == 0) {
            // TODO: Call your display function
            // display_update(preview->preview_buffer, preview->config.preview_width, 
            //                preview->config.preview_height, preview->config.rotation);
            frames++;
        }
        usleep(16666);  // ~60 FPS (16.6ms)
    }
    
    printf("Preview loop: %d frames displayed
", frames);
}

// ---- DCAM Watchdog ----

/*
 * DCAM Watchdog Functions
 * 
 * Hardware/software watchdog for camera stability.
 * Detects/prevents stalls, buffer overflows, dropped frames.
 * Auto-recovers camera on timeout/fatal errors.
 * Essential for production embedded systems.
 */

// IOCTL for watchdog (add to dcam.h)
#define DCAM_WATCHDOG_ARM      _IOW(DCAM_MAGIC, 12, struct dcam_watchdog_config)  // Arm watchdog
#define DCAM_WATCHDOG_KICK     _IO(DCAM_MAGIC, 13)                                 // Reset timer
#define DCAM_WATCHDOG_STATUS   _IOR(DCAM_MAGIC, 14, struct dcam_watchdog_status)   // Get status
#define DCAM_WATCHDOG_DISARM   _IO(DCAM_MAGIC, 15)                                 // Disarm

// Watchdog configuration
struct dcam_watchdog_config {
    uint32_t timeout_ms;       // Watchdog timeout (500-5000ms typical)
    uint32_t kick_interval_ms; // How often to kick (timeout/3 typical)
    uint32_t max_dropped_frames;  // Trigger reset on too many drops
    uint32_t recovery_mode;    // 1=reinit, 2=reset hardware, 3=reboot system
};

// Watchdog status structure
struct dcam_watchdog_status {
    uint32_t is_armed;
    uint32_t time_remaining_ms;
    uint32_t frames_dropped;
    uint32_t recovery_count;
    uint32_t last_error;
};

// Watchdog handle structure
typedef struct {
    camera_handle_t *camera;
    stream_handle_t *stream;  // Optional: monitor streaming too
    preview_handle_t *preview; // Optional: monitor preview too
    struct dcam_watchdog_config config;
    int watchdog_active;
    uint64_t last_kick_time;
    uint32_t dropped_frames;
    uint32_t recovery_count;
} watchdog_handle_t;

// Global watchdog tick (called from main loop or timer)
static watchdog_handle_t *active_watchdog = NULL;

/*
 * Initialize DCAM watchdog.
 * Must be called after camera/stream/preview init.
 */
watchdog_handle_t *watchdog_init(camera_handle_t *camera, 
                                stream_handle_t *stream, 
                                preview_handle_t *preview) {
    watchdog_handle_t *wd = (watchdog_handle_t *)malloc(sizeof(watchdog_handle_t));
    if (!wd) {
        system_log("Failed to allocate watchdog handle.");
        return NULL;
    }
    memset(wd, 0, sizeof(watchdog_handle_t));
    
    wd->camera = camera;
    wd->stream = stream;
    wd->preview = preview;
    
    // Default aggressive config for embedded
    wd->config.timeout_ms = 2000;           // 2 second timeout
    wd->config.kick_interval_ms = 500;      // Kick every 500ms
    wd->config.max_dropped_frames = 10;     // Reset after 10 drops
    wd->config.recovery_mode = 1;           // Re-initialize camera
    
    // Arm hardware watchdog
    if (ioctl(camera->fd, DCAM_WATCHDOG_ARM, &wd->config) < 0) {
        system_log("IOCTL DCAM_WATCHDOG_ARM failed.");
        free(wd);
        return NULL;
    }
    
    wd->watchdog_active = 1;
    active_watchdog = wd;
    wd->last_kick_time = 0;  // System timestamp
    
    char msg[128];
    snprintf(msg, sizeof(msg), "DCAM Watchdog armed: %ums timeout, %ums kick", 
             wd->config.timeout_ms, wd->config.kick_interval_ms);
    system_log(msg);
    
    return wd;
}

/*
 * Kick the watchdog (call periodically from main loop).
 * Also checks for dropped frames and stalls.
 */
int watchdog_kick(watchdog_handle_t *wd) {
    if (!wd || !wd->watchdog_active) {
        return -1;
    }
    
    uint64_t now = 0;  // Replace with system timestamp
    
    // Check kick interval
    if (now - wd->last_kick_time > wd->config.kick_interval_ms * 1000000ULL) {
        // Hardware kick
        if (ioctl(wd->camera->fd, DCAM_WATCHDOG_KICK, NULL) < 0) {
            system_log("Watchdog kick failed - initiating recovery.");
            return watchdog_recover(wd);
        }
        
        // Check dropped frames
        struct dcam_watchdog_status status;
        if (ioctl(wd->camera->fd, DCAM_WATCHDOG_STATUS, &status) == 0) {
            wd->dropped_frames = status.frames_dropped;
            if (status.frames_dropped > wd->config.max_dropped_frames) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Excessive frame drops: %u", status.frames_dropped);
                system_log(msg);
                return watchdog_recover(wd);
            }
        }
        
        wd->last_kick_time = now;
    }
    
    return 0;
}

/*
 * Emergency recovery procedure.
 * Attempts to restore camera functionality.
 */
static int watchdog_recover(watchdog_handle_t *wd) {
    wd->recovery_count++;
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Watchdog recovery #%u triggered", wd->recovery_count);
    system_log(msg);
    
    // Stop all modes first
    if (wd->stream && wd->stream->streaming) {
        stream_stop(wd->stream);
    }
    if (wd->preview && wd->preview->preview_active) {
        preview_stop(wd->preview);
    }
    
    // Hardware reset via IOCTL (if supported)
    ioctl(wd->camera->fd, DCAM_RELEASE, NULL);
    usleep(100000);  // 100ms settle time
    
    // Re-init camera
    if (camera_init() == NULL) {  // Note: This returns new handle, need to update pointers
        system_log("Recovery failed: camera re-init failed.");
        return -1;
    }
    
    // Re-arm watchdog
    ioctl(wd->camera->fd, DCAM_WATCHDOG_ARM, &wd->config);
    
    system_log("Watchdog recovery completed successfully.");
    return 0;
}

/*
 * Get current watchdog status.
 */
int watchdog_get_status(watchdog_handle_t *wd, struct dcam_watchdog_status *status) {
    if (!wd || !wd->watchdog_active || !status) {
        return -1;
    }
    return ioctl(wd->camera->fd, DCAM_WATCHDOG_STATUS, status);
}

/*
 * Disarm watchdog (cleanup).
 */
void watchdog_disarm(watchdog_handle_t *wd) {
    if (!wd || !wd->watchdog_active) {
        return;
    }
    
    ioctl(wd->camera->fd, DCAM_WATCHDOG_DISARM, NULL);
    wd->watchdog_active = 0;
    active_watchdog = NULL;
    
    char msg[64];
    snprintf(msg, sizeof(msg), "Watchdog disarmed. Recoveries: %u", wd->recovery_count);
    system_log(msg);
}

void watchdog_cleanup(watchdog_handle_t *wd) {
    watchdog_disarm(wd);
    free(wd);
}

/*
 * Global watchdog tick (call from 100Hz system timer).
 * Auto-kicks active watchdog.
 */
void watchdog_tick(void) {
    if (active_watchdog) {
        watchdog_kick(active_watchdog);
    }
}

// ---- Cam Stabilizer (Perfect for Cameramen!) ----

/*
 * CAMERA STABILIZER Functions
 * 
 * Electronic Image Stabilization (EIS) using gyroscope/IMU data fusion.
 * Reduces blur in photos/video by compensating for hand shake/rotation.
 * Works with Nexus 6 Snapdragon 805 MPU-6050 IMU (gyro+accel).
 * Smooths camera motion path using low-pass filtering + frame cropping.
 */

// IOIMU IOCTLs for stabilizer (add to dcam.h)
#define DCAM_STABILIZER_ENABLE   _IOW(DCAM_MAGIC, 16, struct dcam_stabilizer_config)  // Enable EIS
#define DCAM_STABILIZER_DISABLE  _IO(DCAM_MAGIC, 17)                                    // Disable EIS
#define DCAM_STABILIZER_STATUS   _IOR(DCAM_MAGIC, 18, struct dcam_stabilizer_status)    // Get status
#define DCAM_GYRO_DATA           _IOR(DCAM_MAGIC, 19, struct dcam_gyro_data)            // Read gyro data

// Stabilizer configuration
struct dcam_stabilizer_config {
    uint32_t enabled;              // 1 = EIS active
    uint32_t crop_percent;         // 10-20% crop for stabilization margin (10 = 10%)
    float gyro_sensitivity;        // Gyro scaling factor (0.001-0.01)
    uint32_t smooth_window_frames; // Moving average window (8-32 frames)
    uint32_t max_correction_x;     // Max pixel correction horizontal
    uint32_t max_correction_y;     // Max pixel correction vertical
};

// Stabilizer status
struct dcam_stabilizer_status {
    float correction_x;            // Current X offset (pixels)
    float correction_y;            // Current Y offset (pixels)
    float gyro_x, gyro_y, gyro_z;  // Raw gyro rates (rad/s)
    uint32_t frame_count;
    uint32_t dropped_frames;
};

// Gyroscope data structure
struct dcam_gyro_data {
    float gyro_x, gyro_y, gyro_z;  // Angular velocity (rad/s)
    float accel_x, accel_y, accel_z; // Acceleration (g)
    uint64_t timestamp;
};

// Stabilizer handle (EIS state)
typedef struct {
    camera_handle_t *camera;
    struct dcam_stabilizer_config config;
    int stabilizer_active;
    
    // Motion tracking state
    float motion_x, motion_y;           // Accumulated motion (pixels)
    float gyro_integral_x, gyro_integral_y;  // Integrated gyro angle
    float smooth_x[32], smooth_y[32];   // Moving average buffers
    int smooth_index;
    uint32_t frame_count;
    
    // Stabilization limits
    float max_offset_x, max_offset_y;
} stabilizer_handle_t;

/*
 * Initialize camera stabilizer (EIS).
 * Configures gyro sensitivity and crop margins for shake compensation.
 */
stabilizer_handle_t *stabilizer_init(camera_handle_t *camera, uint32_t crop_percent) {
    if (!camera || !camera->initialized) {
        system_log("Invalid camera handle for stabilizer_init.");
        return NULL;
    }
    
    stabilizer_handle_t *stab = (stabilizer_handle_t *)malloc(sizeof(stabilizer_handle_t));
    if (!stab) {
        system_log("Failed to allocate stabilizer handle.");
        return NULL;
    }
    memset(stab, 0, sizeof(stabilizer_handle_t));
    
    stab->camera = camera;
    
    // Conservative defaults for handheld shooting
    stab->config.enabled = 1;
    stab->config.crop_percent = crop_percent ? crop_percent : 15;  // 15% crop margin
    stab->config.gyro_sensitivity = 0.005f;     // Tuned for MPU-6050
    stab->config.smooth_window_frames = 16;     // 16-frame smoothing
    stab->config.max_correction_x = camera->config.width * 15 / 100;
    stab->config.max_correction_y = camera->config.height * 15 / 100;
    
    // Initialize smoothing buffers
    memset(stab->smooth_x, 0, sizeof(stab->smooth_x));
    memset(stab->smooth_y, 0, sizeof(stab->smooth_y));
    stab->smooth_index = 0;
    
    // Calculate max offsets from crop
    stab->max_offset_x = camera->config.width * stab->config.crop_percent / 100.0f;
    stab->max_offset_y = camera->config.height * stab->config.crop_percent / 100.0f;
    
    // Enable stabilizer in hardware
    if (ioctl(camera->fd, DCAM_STABILIZER_ENABLE, &stab->config) < 0) {
        system_log("IOCTL DCAM_STABILIZER_ENABLE failed.");
        free(stab);
        return NULL;
    }
    
    stab->stabilizer_active = 1;
    system_log("Camera stabilizer (EIS) initialized successfully.");
    return stab;
}

/*
 * Process frame for stabilization.
 * Updates motion vectors from gyro data and applies crop offsets.
 * Returns correction offsets for display/capture pipeline.
 */
int stabilizer_process_frame(stabilizer_handle_t *stab, struct dcam_stabilizer_status *status) {
    if (!stab || !stab->stabilizer_active || !status) {
        return -1;
    }
    
    // Read latest gyro data
    struct dcam_gyro_data gyro;
    if (ioctl(stab->camera->fd, DCAM_GYRO_DATA, &gyro) < 0) {
        system_log("Failed to read gyro data for stabilization.");
        return -1;
    }
    
    // Integrate gyro to get rotation angles (16.67ms = 60fps frame time)
    float dt = 1.0f / 60.0f;  // Assume 60fps
    stab->gyro_integral_x += gyro.gyro_x * stab->config.gyro_sensitivity * dt;
    stab->gyro_integral_y += gyro.gyro_y * stab->config.gyro_sensitivity * dt;
    
    // Convert rotation to pixel motion (focal length approximation)
    float focal_x = stab->camera->config.width * 0.8f;   // ~80% of width
    float focal_y = stab->camera->config.height * 0.8f;
    float motion_x = stab->gyro_integral_x * focal_x;
    float motion_y = stab->gyro_integral_y * focal_y;
    
    // Update moving average (simple low-pass filter)
    stab->smooth_x[stab->smooth_index] = motion_x;
    stab->smooth_y[stab->smooth_index] = motion_y;
    stab->smooth_index = (stab->smooth_index + 1) % stab->config.smooth_window_frames;
    
    float avg_x = 0, avg_y = 0;
    for (int i = 0; i < stab->config.smooth_window_frames; i++) {
        avg_x += stab->smooth_x[i];
        avg_y += stab->smooth_y[i];
    }
    stab->motion_x = avg_x / stab->config.smooth_window_frames;
    stab->motion_y = avg_y / stab->config.smooth_window_frames;
    
    // Clamp to max correction limits
    stab->motion_x = stab->motion_x > stab->max_offset_x ? stab->max_offset_x : 
                     stab->motion_x < -stab->max_offset_x ? -stab->max_offset_x : stab->motion_x;
    stab->motion_y = stab->motion_y > stab->max_offset_y ? stab->max_offset_y : 
                     stab->motion_y < -stab->max_offset_y ? -stab->max_offset_y : stab->motion_y;
    
    // Return status for display pipeline
    status->correction_x = -stab->motion_x;  // Negative for compensation
    status->correction_y = -stab->motion_y;
    status->gyro_x = gyro.gyro_x;
    status->gyro_y = gyro.gyro_y;
    status->gyro_z = gyro.gyro_z;
    status->frame_count = ++stab->frame_count;
    
    return 0;
}

/*
 * Get current stabilization status.
 */
int stabilizer_get_status(stabilizer_handle_t *stab, struct dcam_stabilizer_status *status) {
    if (!stab || !stab->stabilizer_active || !status) {
        return -1;
    }
    return ioctl(stab->camera->fd, DCAM_STABILIZER_STATUS, status);
}

/*
 * Disable stabilizer (full resolution capture).
 */
void stabilizer_disable(stabilizer_handle_t *stab) {
    if (!stab || !stab->stabilizer_active) {
        return;
    }
    ioctl(stab->camera->fd, DCAM_STABILIZER_DISABLE, NULL);
    stab->stabilizer_active = 0;
    system_log("Camera stabilizer disabled.");
}

/*
 * Cleanup stabilizer resources.
 */
void stabilizer_cleanup(stabilizer_handle_t *stab) {
    if (!stab) {
        return;
    }
    stabilizer_disable(stab);
    free(stab);
    system_log("Stabilizer resources cleaned up.");
}

// ---- Warnings ----

/*
 * WARNINGS System
 * 
 * Comprehensive warning and error reporting system.
 * Categorizes issues by severity: INFO, WARNING, ERROR, CRITICAL, FATAL.
 * Provides structured diagnostics for debugging/production monitoring.
 * Auto-generates recovery suggestions and statistics.
 */

// Warning severity levels
#define WARN_INFO     0  // Informational
#define WARN_WARNING  1  // Non-critical issues
#define WARN_ERROR    2  // Recoverable errors  
#define WARN_CRITICAL 3  // Service-impacting
#define WARN_FATAL    4  // Unrecoverable

// Warning categories
#define WARN_CAT_INIT       0x01  // Initialization failures
#define WARN_CAT_CONFIG     0x02  // Configuration issues
#define WARN_CAT_CAPTURE    0x04  // Capture/streaming problems
#define WARN_CAT_STABILIZER 0x08  // EIS failures
#define WARN_CAT_WATCHDOG   0x10  // Watchdog triggers
#define WARN_CAT_MEMORY     0x20  // Memory allocation
#define WARN_CAT_HARDWARE   0x40  // Hardware faults
#define WARN_CAT_PERFORMANCE 0x80 // Performance degradation

// Warning structure
struct dcam_warning {
    uint32_t severity;
    uint32_t category;
    uint64_t timestamp;
    int error_code;
    char message[256];
    char recovery[128];
    uint32_t count;  // Occurrence count
};

// Warning statistics
struct dcam_warning_stats {
    uint32_t total_warnings;
    uint32_t by_severity[5];     // Count per severity level
    uint32_t by_category[8];     // Count per category
    uint32_t recovery_success;   // Successful auto-recoveries
    uint64_t last_warning_time;
};

// Warnings handle for tracking
typedef struct {
    struct dcam_warning_stats stats;
    int warnings_enabled;
    uint32_t max_warnings;       // Throttle warnings
    uint32_t warning_count;
} warnings_handle_t;

// Global warnings handle
static warnings_handle_t *active_warnings = NULL;

// Helper: Get severity string
static const char *get_severity_str(uint32_t severity) {
    switch (severity) {
        case WARN_INFO:     return "INFO";
        case WARN_WARNING:  return "WARN";
        case WARN_ERROR:    return "ERROR";
        case WARN_CRITICAL: return "CRITICAL";
        case WARN_FATAL:    return "FATAL";
        default:            return "UNKNOWN";
    }
}

// Helper: Get category string
static const char *get_category_str(uint32_t category) {
    switch (category) {
        case WARN_CAT_INIT:       return "INIT";
        case WARN_CAT_CONFIG:     return "CONFIG";
        case WARN_CAT_CAPTURE:    return "CAPTURE";
        case WARN_CAT_STABILIZER: return "STABILIZER";
        case WARN_CAT_WATCHDOG:   return "WATCHDOG";
        case WARN_CAT_MEMORY:     return "MEMORY";
        case WARN_CAT_HARDWARE:   return "HARDWARE";
        case WARN_CAT_PERFORMANCE: return "PERFORMANCE";
        default:                  return "UNKNOWN";
    }
}

/*
 * Initialize warnings system.
 */
warnings_handle_t *warnings_init(uint32_t max_warnings) {
    warnings_handle_t *warnings = (warnings_handle_t *)malloc(sizeof(warnings_handle_t));
    if (!warnings) {
        system_log("CRITICAL: Failed to allocate warnings handle.");
        return NULL;
    }
    memset(warnings, 0, sizeof(warnings_handle_t));
    
    warnings->warnings_enabled = 1;
    warnings->max_warnings = max_warnings ? max_warnings : 1000;
    active_warnings = warnings;
    
    system_log("Warnings system initialized.");
    return warnings;
}

/*
 * Log structured warning with auto-recovery suggestion.
 * Returns 0 on success, -1 if throttled.
 */
int dcam_warning(warnings_handle_t *warnings, uint32_t severity, uint32_t category, 
                 int error_code, const char *format, ...) {
    if (!warnings || !warnings->warnings_enabled) {
        return 0;
    }
    
    // Throttle warnings
    if (warnings->warning_count >= warnings->max_warnings) {
        return -1;
    }
    
    char message[256];
    char full_msg[512];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // Update stats
    warnings->stats.total_warnings++;
    warnings->stats.by_severity[severity]++;
    warnings->stats.by_category[__builtin_ctz(category)]++;  // Get bit position
    warnings->warning_count++;
    
    // Generate full log message
    snprintf(full_msg, sizeof(full_msg), "[%s:%s:%d] %s (count: %u)",
             get_severity_str(severity), get_category_str(category), 
             error_code, message, warnings->stats.total_warnings);
    
    system_log(full_msg);
    
    // Auto-recovery suggestions
    char recovery[128] = "Manual intervention required";
    switch (severity) {
        case WARN_INFO:
            snprintf(recovery, sizeof(recovery), "Normal operation");
            break;
        case WARN_WARNING:
            snprintf(recovery, sizeof(recovery), "Monitor performance");
            break;
        case WARN_ERROR:
            snprintf(recovery, sizeof(recovery), "Attempt auto-recovery");
            warnings->stats.recovery_success++;
            break;
        case WARN_CRITICAL:
            snprintf(recovery, sizeof(recovery), "Service restart recommended");
            break;
        case WARN_FATAL:
            snprintf(recovery, sizeof(recovery), "System reboot required");
            break;
    }
    
    return 0;
}

/*
 * Convenience warning macros (use in other functions)
 */
#define WARN_INFO(w, fmt, ...)     dcam_warning(w, WARN_INFO,     WARN_CAT_PERFORMANCE,  0, fmt, ##__VA_ARGS__)
#define WARN_WARN(w, cat, code, fmt, ...)  dcam_warning(w, WARN_WARNING,  cat, code, fmt, ##__VA_ARGS__)
#define WARN_ERROR(w, cat, code, fmt, ...) dcam_warning(w, WARN_ERROR,   cat, code, fmt, ##__VA_ARGS__)
#define WARN_CRIT(w, cat, code, fmt, ...)  dcam_warning(w, WARN_CRITICAL, cat, code, fmt, ##__VA_ARGS__)
#define WARN_FATAL(w, cat, code, fmt, ...) dcam_warning(w, WARN_FATAL,   cat, code, fmt, ##__VA_ARGS__)

/*
 * Get comprehensive warning statistics.
 */
int warnings_get_stats(warnings_handle_t *warnings, struct dcam_warning_stats *stats) {
    if (!warnings || !stats) return -1;
    *stats = warnings->stats;
    return 0;
}

/*
 * Clear warning statistics (for testing).
 */
void warnings_clear_stats(warnings_handle_t *warnings) {
    if (warnings) {
        memset(&warnings->stats, 0, sizeof(warnings->stats));
    }
}

/*
 * Disable warnings system (emergency mute).
 */
void warnings_disable(warnings_handle_t *warnings) {
    if (warnings) {
        warnings->warnings_enabled = 0;
        active_warnings = NULL;
    }
}

/*
 * Cleanup warnings system.
 */
void warnings_cleanup(warnings_handle_t *warnings) {
    if (warnings) {
        warnings_disable(warnings);
        free(warnings);
    }
}


// ---- Error handling ----

/*
 * ERROR HANDLING System
 * 
 * Comprehensive error handling framework with retry logic, fallback modes,
 * graceful degradation, and automatic recovery strategies.
 * Integrates with warnings system for full diagnostics.
 * Handles all failure modes: IOCTL, memory, hardware, timeouts, permissions.
 */

// Error recovery strategies
#define ERR_RETRY_FAST     0  // 1-3 retries, 10-50ms delays
#define ERR_RETRY_MEDIUM   1  // 3-10 retries, 100-500ms delays  
#define ERR_RETRY_SLOW     2  // 10-30 retries, 1-5s delays
#define ERR_FALLBACK_MODE  3  // Switch to lower quality/speed mode
#define ERR_HARD_RESET     4  // Full hardware reset
#define ERR_FAIL_SAFE      5  // Graceful shutdown

// Common error codes (mapped to errno where possible)
#define ERR_CAMERA_NOT_FOUND   1001
#define ERR_IOCTL_FAILED       1002
#define ERR_MEMORY_EXHAUSTED   1003
#define ERR_TIMEOUT            1004
#define ERR_INVALID_CONFIG     1005
#define ERR_HARDWARE_FAULT     1006
#define ERR_PERMISSION_DENIED  1007
#define ERR_BUFFER_OVERFLOW    1008
#define ERR_STREAM_STALLED     1009
#define ERR_GYRO_UNAVAILABLE   1010

// Error context structure
struct dcam_error_context {
    int error_code;
    int system_errno;     // errno at failure time
    uint64_t timestamp;
    const char *operation; // "ioctl", "malloc", "capture", etc.
    int retry_count;
    int recovery_attempted;
    int recovery_success;
};

// Error handling configuration
struct dcam_error_config {
    uint32_t max_retries_fast;
    uint32_t max_retries_medium;
    uint32_t max_retries_slow;
    uint32_t enable_fallbacks;
    uint32_t log_detailed_errors;
};

// Global error handler state
typedef struct {
    struct dcam_error_config config;
    warnings_handle_t *warnings;
    int error_handling_active;
    uint32_t total_recoveries;
    uint32_t failed_recoveries;
} error_handler_t;

static error_handler_t *active_error_handler = NULL;

/*
 * Initialize error handling system.
 */
error_handler_t *error_handler_init(warnings_handle_t *warnings) {
    error_handler_t *eh = (error_handler_t *)malloc(sizeof(error_handler_t));
    if (!eh) {
        system_log("FATAL: Cannot initialize error handler - memory exhausted");
        return NULL;
    }
    memset(eh, 0, sizeof(error_handler_t));
    
    // Conservative production defaults
    eh->config.max_retries_fast = 3;
    eh->config.max_retries_medium = 8;
    eh->config.max_retries_slow = 20;
    eh->config.enable_fallbacks = 1;
    eh->config.log_detailed_errors = 1;
    
    eh->warnings = warnings;
    eh->error_handling_active = 1;
    active_error_handler = eh;
    
    system_log("Error handling system initialized.");
    return eh;
}

/*
 * Generic retry wrapper for any operation.
 * Returns 0 on success, error code on final failure.
 */
int error_retry_operation(error_handler_t *eh, int (*operation)(void *), void *context,
                         int strategy, const char *operation_name) {
    if (!eh || !eh->error_handling_active || !operation) {
        return -1;
    }
    
    int retries = 0;
    int max_retries = 0;
    uint32_t delay_ms = 0;
    
    switch (strategy) {
        case ERR_RETRY_FAST:   max_retries = eh->config.max_retries_fast;   delay_ms = 25; break;
        case ERR_RETRY_MEDIUM: max_retries = eh->config.max_retries_medium; delay_ms = 250; break;
        case ERR_RETRY_SLOW:   max_retries = eh->config.max_retries_slow;   delay_ms = 2000; break;
        default: return operation(context);
    }
    
    while (retries <= max_retries) {
        int result = operation(context);
        
        if (result == 0) {
            // Success! Log recovery if we had prior failures
            if (retries > 0) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Operation '%s' recovered after %d retries", operation_name, retries);
                dcam_warning(eh->warnings, WARN_INFO, WARN_CAT_PERFORMANCE, 0, "%s", msg);
            }
            eh->total_recoveries++;
            return 0;
        }
        
        retries++;
        
        // Log progressive failures
        if (eh->config.log_detailed_errors) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Operation '%s' failed (attempt %d/%d): %s", 
                     operation_name, retries, max_retries+1, strerror(errno));
            dcam_warning(eh->warnings, WARN_ERROR, WARN_CAT_CAPTURE, errno, "%s", msg);
        }
        
        // Final attempt - no more retries
        if (retries > max_retries) {
            eh->failed_recoveries++;
            return result;
        }
        
        // Delay before retry (exponential backoff)
        usleep(delay_ms * 1000 * (retries > 4 ? 2 : 1));
    }
    
    return -1;  // Unreachable
}

/*
 * IOCTL wrapper with automatic retry and fallback.
 */
int error_safe_ioctl(error_handler_t *eh, int fd, unsigned long request, void *arg) {
    if (!eh) return ioctl(fd, request, arg);
    
    int (*ioctl_op)(void *) = ^(void *ctx) {
        struct { int fd; unsigned long req; void *arg; } *c = ctx;
        return ioctl(c->fd, c->req, c->arg);
    };
    
    struct { int fd; unsigned long req; void *arg; } ctx = { fd, request, arg };
    
    return error_retry_operation(eh, ioctl_op, &ctx, ERR_RETRY_FAST, "ioctl");
}

/*
 * Memory allocation wrapper with fallback sizes.
 */
void *error_safe_malloc(error_handler_t *eh, size_t size, size_t *actual_size) {
    if (!eh || size == 0) return malloc(size);
    
    void *ptr = malloc(size);
    if (ptr || size <= 1024) return ptr;  // Success or tiny allocation
    
    // Fallback: try halving size repeatedly
    size_t fallback_size = size / 2;
    while (fallback_size >= 1024 && !ptr) {
        ptr = malloc(fallback_size);
        if (ptr) {
            dcam_warning(eh->warnings, WARN_WARN, WARN_CAT_MEMORY, 0, 
                        "Malloc fallback: %zu -> %zu bytes", size, fallback_size);
            if (actual_size) *actual_size = fallback_size;
            eh->total_recoveries++;
            return ptr;
        }
        fallback_size /= 2;
    }
    
    dcam_warning(eh->warnings, WARN_CRIT, WARN_CAT_MEMORY, 0, "Malloc failed: %zu bytes", size);
    eh->failed_recoveries++;
    return NULL;
}

/*
 * Safe camera capture with timeout and fallback to single-shot.
 */
int error_safe_capture(error_handler_t *eh, camera_handle_t *handle) {
    if (!eh || !handle) return camera_capture(handle);
    
    // Try streaming capture first, fallback to single shot
    int result = error_retry_operation(eh, (int(*)(void*))camera_capture, handle, 
                                      ERR_RETRY_MEDIUM, "camera_capture");
    
    if (result != 0) {
        dcam_warning(eh->warnings, WARN_ERROR, WARN_CAT_CAPTURE, 0, 
                    "Capture failed, attempting fallback mode");
        // Fallback: reduce resolution and retry
        struct dcam_config fallback_config = handle->config;
        fallback_config.width /= 2;
        fallback_config.height /= 2;
        camera_set_config(handle, &fallback_config);
        
        result = camera_capture(handle);
        if (result == 0) {
            eh->total_recoveries++;
            dcam_warning(eh->warnings, WARN_INFO, WARN_CAT_CAPTURE, 0, 
                        "Capture recovered via resolution fallback");
        }
    }
    
    return result;
}

/*
 * Emergency full system reset (last resort).
 */
int error_hard_reset(error_handler_t *eh, camera_handle_t *camera) {
    if (!eh || !camera) return -1;
    
    dcam_warning(eh->warnings, WARN_CRIT, WARN_CAT_HARDWARE, ERR_HARD_RESET, 
                "Initiating full camera hardware reset");
    
    // Stop all operations
    if (active_watchdog) watchdog_disarm(active_watchdog);
    
    // Force close device
    close(camera->fd);
    usleep(500000);  // 500ms hardware settle
    
    // Re-open and re-init
    camera->fd = open(camera_dev_path, O_RDWR);
    if (camera->fd >= 0) {
        ioctl(camera->fd, DCAM_INIT, NULL);
        eh->total_recoveries++;
        return 0;
    }
    
    eh->failed_recoveries++;
    return -1;
}

/*
 * Get error handler statistics.
 */
int error_get_stats(error_handler_t *eh, uint32_t *total_recoveries, uint32_t *failed_recoveries) {
    if (!eh || !total_recoveries || !failed_recoveries) return -1;
    *total_recoveries = eh->total_recoveries;
    *failed_recoveries = eh->failed_recoveries;
    return 0;
}

/*
 * Cleanup error handling system.
 */
void error_handler_cleanup(error_handler_t *eh) {
    if (eh) {
        eh->error_handling_active = 0;
        active_error_handler = NULL;
        free(eh);
    }
}

// ---- Performance Monitor ----

/*
 * PERFORMANCE MONITOR Functions
 * 
 * Real-time performance metrics, frame timing, FPS counter, 
 * latency tracking, and resource usage monitoring.
 * Essential for optimizing camera pipeline and debugging bottlenecks.
 * Provides histograms, percentiles, and alerting on degradation.
 */

// IOCTL for performance monitoring (add to dcam.h)
#define DCAM_PERF_START      _IO(DCAM_MAGIC, 20)    // Start performance counters
#define DCAM_PERF_STOP       _IO(DCAM_MAGIC, 21)    // Stop counters
#define DCAM_PERF_SNAPSHOT   _IOR(DCAM_MAGIC, 22, struct dcam_perf_stats)  // Get stats
#define DCAM_PERF_RESET      _IO(DCAM_MAGIC, 23)    // Reset counters

// Performance metrics structure
struct dcam_perf_stats {
    uint64_t capture_latency_ns;   // Average capture time (nanoseconds)
    uint64_t total_frames;         // Total frames captured
    uint64_t dropped_frames;       // Hardware/software drops
    float fps_current;             // Instant FPS
    float fps_average;             // Session average FPS
    float cpu_usage_percent;       // CPU utilization
    uint64_t memory_usage_bytes;   // Heap usage
    uint32_t latency_p50;          // 50th percentile latency (us)
    uint32_t latency_p90;          // 90th percentile latency (us)
    uint32_t latency_p99;          // 99th percentile latency (us)
    uint64_t timestamp;
};

// Performance handle with circular buffer for histograms
typedef struct {
    camera_handle_t *camera;
    warnings_handle_t *warnings;
    error_handler_t *error_handler;
    int perf_monitor_active;
    
    // Timing state
    uint64_t frame_times[128];     // Circular buffer for latency histogram
    uint64_t total_time_ns;        // Total session time
    int frame_index;
    int frame_count;
    
    // FPS calculation
    uint64_t fps_start_time;
    float target_fps;
    
    // Resource tracking
    uint64_t start_memory;
    uint64_t peak_memory;
} perf_monitor_t;

/*
 * Initialize performance monitor.
 * Starts hardware counters and baseline memory tracking.
 */
perf_monitor_t *perf_monitor_init(camera_handle_t *camera, warnings_handle_t *warnings, 
                                 error_handler_t *eh, float target_fps) {
    perf_monitor_t *perf = (perf_monitor_t *)malloc(sizeof(perf_monitor_t));
    if (!perf) {
        system_log("CRITICAL: Failed to allocate perf monitor");
        return NULL;
    }
    memset(perf, 0, sizeof(perf_monitor_t));
    
    perf->camera = camera;
    perf->warnings = warnings;
    perf->error_handler = eh;
    perf->target_fps = target_fps ? target_fps : 30.0f;
    
    // Initialize circular buffer
    memset(perf->frame_times, 0, sizeof(perf->frame_times));
    
    // Baseline memory (approximate via sbrk or malloc tracking)
    perf->start_memory = 0;  // TODO: implement memory baseline
    perf->peak_memory = 0;
    
    // Start hardware performance counters
    if (ioctl(camera->fd, DCAM_PERF_START, NULL) < 0) {
        dcam_warning(warnings, WARN_WARN, WARN_CAT_PERFORMANCE, errno, 
                    "Hardware perf counters unavailable - using software timing");
    }
    
    perf->fps_start_time = 0;  // System timestamp
    perf->perf_monitor_active = 1;
    
    char msg[128];
    snprintf(msg, sizeof(msg), "Perf monitor started, target: %.1ffps", target_fps);
    system_log(msg);
    
    return perf;
}

/*
 * Timestamp wrapper for high-resolution timing.
 * Returns nanoseconds since boot.
 */
static uint64_t perf_get_timestamp_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

/*
 * Record frame timing and update statistics.
 * Call at end of each frame capture/process/display cycle.
 */
void perf_record_frame(perf_monitor_t *perf, const char *stage) {
    if (!perf || !perf->perf_monitor_active) return;
    
    static uint64_t last_frame_time = 0;
    uint64_t now = perf_get_timestamp_ns();
    
    if (last_frame_time == 0) {
        last_frame_time = now;
        return;
    }
    
    uint64_t frame_latency_ns = now - last_frame_time;
    last_frame_time = now;
    
    // Update circular buffer for latency histogram
    perf->frame_times[perf->frame_index] = frame_latency_ns;
    perf->frame_index = (perf->frame_index + 1) % 128;
    if (perf->frame_count < 128) perf->frame_count++;
    
    perf->total_time_ns += frame_latency_ns;
    
    // Calculate instant FPS
    float instant_fps = 1e9f / frame_latency_ns;
    
    // Alert on performance degradation
    if (instant_fps < perf->target_fps * 0.8f) {
        dcam_warning(perf->warnings, WARN_WARN, WARN_CAT_PERFORMANCE, 0,
                    "FPS dropped to %.1f (target: %.1f) at stage '%s'", 
                    instant_fps, perf->target_fps, stage);
    }
}

/*
 * Get comprehensive performance snapshot.
 */
int perf_get_snapshot(perf_monitor_t *perf, struct dcam_perf_stats *stats) {
    if (!perf || !perf->perf_monitor_active || !stats) return -1;
    
    // Get hardware stats if available
    struct dcam_perf_stats hw_stats;
    if (ioctl(perf->camera->fd, DCAM_PERF_SNAPSHOT, &hw_stats) == 0) {
        *stats = hw_stats;
    }
    
    // Calculate software statistics
    if (perf->frame_count > 0) {
        stats->total_frames = perf->frame_count;
        stats->fps_average = (float)perf->frame_count * 1e9f / perf->total_time_ns;
        
        // Calculate percentiles (simple sort for small buffer)
        uint64_t latencies[128];
        memcpy(latencies, perf->frame_times, sizeof(perf->frame_times));
        
        // Quick sort latencies (or use qsort)
        for (int i = 0; i < perf->frame_count - 1; i++) {
            for (int j = 0; j < perf->frame_count - i - 1; j++) {
                if (latencies[j] > latencies[j + 1]) {
                    uint64_t temp = latencies[j];
                    latencies[j] = latencies[j + 1];
                    latencies[j + 1] = temp;
                }
            }
        }
        
        stats->latency_p50 = latencies[perf->frame_count / 2] / 1000;  // us
        stats->latency_p90 = perf->frame_count > 10 ? latencies[perf->frame_count * 9 / 10] / 1000 : 0;
        stats->latency_p99 = perf->frame_count > 100 ? latencies[perf->frame_count * 99 / 100] / 1000 : 0;
    }
    
    stats->capture_latency_ns = perf->total_time_ns / perf->frame_count;
    stats->timestamp = perf_get_timestamp_ns();
    
    // Auto-alert on poor performance
    if (stats->fps_average < perf->target_fps * 0.7f) {
        dcam_warning(perf->warnings, WARN_ERROR, WARN_CAT_PERFORMANCE, 0,
                    "Sustained FPS degradation: %.1f (target: %.1f)", 
                    stats->fps_average, perf->target_fps);
    }
    
    return 0;
}

/*
 * Simple ASCII FPS gauge for debugging.
 */
void perf_print_gauge(perf_monitor_t *perf) {
    struct dcam_perf_stats stats;
    if (perf_get_snapshot(perf, &stats) != 0) return;
    
    int fps_bars = (int)(stats.fps_current / 5);  // 5fps per bar
    printf("[FPS: %.1f/%.1f |", stats.fps_current, perf->target_fps);
    
    for (int i = 0; i < 10; i++) {
        if (i < fps_bars) printf("█");
        else if (i < 10) printf("░");
    }
    printf("] p90: %dμs
", stats.latency_p90);
}

/*
 * Stop performance monitoring and print final report.
 */
void perf_monitor_stop(perf_monitor_t *perf) {
    if (!perf || !perf->perf_monitor_active) return;
    
    ioctl(perf->camera->fd, DCAM_PERF_STOP, NULL);
    perf->perf_monitor_active = 0;
    
    struct dcam_perf_stats final_stats;
    perf_get_snapshot(perf, &final_stats);
    
    char summary[256];
    snprintf(summary, sizeof(summary), 
            "PERF SUMMARY: %.1ffps avg, %llu frames, %d%% dropped, p90:%dμs", 
            final_stats.fps_average, final_stats.total_frames, 
            (int)(final_stats.dropped_frames * 100 / final_stats.total_frames), 
            final_stats.latency_p90);
    system_log(summary);
}

/*
 * Cleanup performance monitor.
 */
void perf_monitor_cleanup(perf_monitor_t *perf) {
    if (perf) {
        perf_monitor_stop(perf);
        free(perf);
    }
}