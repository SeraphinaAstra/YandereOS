/*
  YandereOS Kernel - V3.5
  A functional kernel for Arduino Giga R1 WiFi
  
  Features:
  - Cooperative multitasking with watchdog timer protection
  - Memory management with proper compaction (pointer-safe via handles)
  - IPC (message queues, semaphores)
  - Device Driver Interface (DDI) for GPIO, I2C, SPI
  - Stack traces for panic debugging
  - Fully backward compatible with v2.0
*/

#ifndef KERNEL_H
#define KERNEL_H

#include <Arduino.h>
#include <SD.h>
#include <Wire.h>
#include <SPI.h>

// ============================================================================
// SYSTEM CALL DEFINITIONS
// ============================================================================

enum SyscallType {
  // File operations
  SYS_FILE_OPEN = 0,
  SYS_FILE_CLOSE,
  SYS_FILE_READ,
  SYS_FILE_WRITE,
  SYS_FILE_DELETE,
  SYS_FILE_EXISTS,
  SYS_FILE_SIZE,
  
  // Directory operations
  SYS_DIR_OPEN,
  SYS_DIR_READ,
  SYS_DIR_CLOSE,
  SYS_DIR_CREATE,
  SYS_DIR_REMOVE,
  SYS_DIR_REWIND,
  
  // Memory operations
  SYS_MEM_ALLOC,
  SYS_MEM_FREE,
  SYS_MEM_INFO,
  SYS_MEM_COMPACT,
  
  // Display operations (not implemented yet)
  SYS_DISPLAY_CLEAR,
  SYS_DISPLAY_PIXEL,
  SYS_DISPLAY_TEXT,
  SYS_DISPLAY_RECT,
  SYS_DISPLAY_UPDATE,
  
  // Task operations
  SYS_TASK_CREATE,
  SYS_TASK_KILL,
  SYS_TASK_YIELD,
  SYS_TASK_SLEEP,
  SYS_TASK_LIST,
  
  // IPC operations (NEW)
  SYS_IPC_SEND,
  SYS_IPC_RECEIVE,
  SYS_IPC_POLL,
  SYS_SEM_CREATE,
  SYS_SEM_WAIT,
  SYS_SEM_POST,
  SYS_SEM_DESTROY,
  
  // GPIO operations (NEW)
  SYS_GPIO_PINMODE,
  SYS_GPIO_WRITE,
  SYS_GPIO_READ,
  SYS_GPIO_ANALOG_READ,
  SYS_GPIO_ANALOG_WRITE,
  
  // I2C operations (NEW)
  SYS_I2C_BEGIN,
  SYS_I2C_WRITE,
  SYS_I2C_READ,
  SYS_I2C_REQUEST,
  
  // SPI operations (NEW)
  SYS_SPI_BEGIN,
  SYS_SPI_TRANSFER,
  SYS_SPI_END,
  
  // System operations
  SYS_GET_TIME,
  SYS_PRINT,
  SYS_DBG_PRINT
};

// System call result codes
enum SyscallResult {
  SYS_OK = 0,
  SYS_ERR_INVALID_CALL = -1,
  SYS_ERR_PERMISSION = -2,
  SYS_ERR_NO_MEMORY = -3,
  SYS_ERR_NOT_FOUND = -4,
  SYS_ERR_IO_ERROR = -5,
  SYS_ERR_INVALID_PARAM = -6,
  SYS_ERR_TIMEOUT = -7,
  SYS_ERR_WOULD_BLOCK = -8
};

// Configuration
#define MAX_TASKS 8
#define MAX_FILE_HANDLES 16
#define MAX_DIR_HANDLES 4
#define MAX_MESSAGE_QUEUE_SIZE 16
#define MAX_SEMAPHORES 8
#define MAX_STACK_TRACE_DEPTH 8

// Watchdog configuration
#define WATCHDOG_TIMEOUT_MS 5000  // 5 seconds without yield = force reschedule

// Heap size based on board
#ifdef ARDUINO_GIGA
  #define KERNEL_HEAP_SIZE (512 * 1024)  // 512KB for Giga R1 (STM32H747, 1MB RAM)
#elif defined(ARDUINO_ARCH_RP2040)
  #define KERNEL_HEAP_SIZE (128 * 1024)  // 128KB for RP2040 (264KB total RAM)
#elif defined(ARDUINO_AVR_MEGA2560)
  #define KERNEL_HEAP_SIZE (4 * 1024)    // 4KB for Mega (8KB total RAM)
#elif defined(ARDUINO_ARCH_SAMD)
  #define KERNEL_HEAP_SIZE (16 * 1024)   // 16KB for SAMD21 (32KB RAM)
#elif defined(ARDUINO_ARCH_ESP32)
  #define KERNEL_HEAP_SIZE (256 * 1024)  // 256KB for ESP32 (320KB+ RAM)
#elif defined(ARDUINO_ARCH_ESP8266)
  #define KERNEL_HEAP_SIZE (32 * 1024)   // 32KB for ESP8266 (80KB RAM)
#else
  #define KERNEL_HEAP_SIZE (2 * 1024)    // 2KB conservative default (Uno-class)
#endif

// SD Card Configuration
#ifdef ARDUINO_ARCH_RP2040
  #define SD_CS_PIN 17  // RP2040 Pico default
#elif defined(ARDUINO_AVR_MEGA2560)
  #define SD_CS_PIN 53  // Arduino Mega default
#elif defined(ARDUINO_GIGA)
  #define SD_CS_PIN 10  // Giga R1 WiFi default
#else
  #define SD_CS_PIN 10  // Generic Arduino default (Uno, etc.)
#endif

// ============================================================================
// TASK MANAGEMENT
// ============================================================================

enum TaskState {
  TASK_EMPTY = 0,
  TASK_READY,
  TASK_RUNNING,
  TASK_SLEEPING,
  TASK_BLOCKED,
  TASK_ZOMBIE
};

// Stack trace entry
struct StackFrame {
  void* returnAddress;
  const char* functionName;
};

struct Task {
  int id;
  const char* name;
  TaskState state;
  void (*entryPoint)();
  
  // Scheduling
  uint32_t sleepUntil;
  uint32_t lastRun;
  uint32_t lastYield;  // NEW: For watchdog
  int priority;
  
  // Resource tracking
  bool fileHandles[MAX_FILE_HANDLES];
  bool dirHandles[MAX_DIR_HANDLES];
  size_t memoryUsed;
  
  // Stack trace (NEW)
  StackFrame stackTrace[MAX_STACK_TRACE_DEPTH];
  int stackTraceDepth;
  
  // Permissions
  bool canAccessSD;
  bool canAccessDisplay;
  bool canCreateTasks;
  bool canAccessGPIO;     // NEW
  bool canAccessI2C;      // NEW
  bool canAccessSPI;      // NEW
};

// ============================================================================
// MEMORY MANAGEMENT - Handle-based for safe compaction
// ============================================================================

struct MemoryBlock {
  size_t size;
  int ownerTaskId;
  bool inUse;
  int handleId;  // NEW: For tracking during compaction
};

// ============================================================================
// IPC - Message Queues (NEW)
// ============================================================================

struct Message {
  int fromTaskId;
  int toTaskId;
  uint8_t data[64];
  size_t length;
  uint32_t timestamp;
  bool valid;
};

struct MessageQueue {
  Message messages[MAX_MESSAGE_QUEUE_SIZE];
  int head;
  int tail;
  int count;
};

// ============================================================================
// IPC - Semaphores (NEW)
// ============================================================================

struct Semaphore {
  int value;
  int maxValue;
  bool inUse;
  int ownerTaskId;
  const char* name;
};

// ============================================================================
// FILE SYSTEM ABSTRACTION
// ============================================================================

struct FileHandle {
  File file;
  int ownerTaskId;
  bool inUse;
  bool canWrite;
};

struct DirHandle {
  File dir;
  int ownerTaskId;
  bool inUse;
};

struct DirEntry {
  char name[64];
  bool isDirectory;
  size_t size;
};

// ============================================================================
// DEVICE DRIVER INTERFACE (NEW)
// ============================================================================

// I2C transaction
struct I2CTransaction {
  uint8_t address;
  uint8_t* data;
  size_t length;
  bool write;
};

// SPI transaction
struct SPITransaction {
  uint8_t* txData;
  uint8_t* rxData;
  size_t length;
  uint32_t frequency;
};

// ============================================================================
// KERNEL CLASS
// ============================================================================

class Kernel {
private:
  // Task management
  static Task tasks[MAX_TASKS];
  static int currentTaskId;
  static int nextTaskId;
  
  // Memory management
  static uint8_t kernelHeap[KERNEL_HEAP_SIZE];
  static size_t heapUsed;
  
  // File system
  static FileHandle fileHandles[MAX_FILE_HANDLES];
  static DirHandle dirHandles[MAX_DIR_HANDLES];
  static bool sdInitialized;
  
  // IPC (NEW)
  static MessageQueue messageQueues[MAX_TASKS];
  static Semaphore semaphores[MAX_SEMAPHORES];
  
  // Watchdog (NEW)
  static bool watchdogEnabled;
  static uint32_t watchdogLastCheck;
  
  // System state
  static bool initialized;
  static uint32_t bootTime;
  
  // Private methods
  static Task* getCurrentTask();
  static Task* getTask(int taskId);
  static int allocateTaskId();
  static int allocateFileHandle();
  static int allocateDirHandle();
  static void freeFileHandle(int handle);
  static void freeDirHandle(int handle);
  
  // Memory management internals
  static void* allocateMemoryInternal(size_t size, int taskId);
  static void freeMemoryInternal(void* ptr);
  static void compactMemory();
  static MemoryBlock* getBlockHeader(void* ptr);
  
  // Watchdog (NEW)
  static void checkWatchdog();
  
  // Stack tracing (NEW)
  static void captureStackTrace(Task* task);
  static void printStackTrace(Task* task);
  
  // IPC internals (NEW)
  static int allocateSemaphore();
  
public:
  // Initialization
  static bool init();
  static void panic(const char* message);
  
  // Task scheduling
  static int createTask(const char* name, void (*entryPoint)());
  static void killTask(int taskId);
  static void schedule();
  static void yield();
  static void sleep(uint32_t ms);
  
  // Watchdog (NEW)
  static void enableWatchdog(bool enable);
  static void feedWatchdog();
  
  // System calls
  static int syscall(SyscallType type, void* arg1 = nullptr, void* arg2 = nullptr, 
                     void* arg3 = nullptr, void* arg4 = nullptr);
  
  // File operations
  static int fileOpen(const char* path, bool write = false);
  static int fileClose(int handle);
  static int fileRead(int handle, void* buffer, size_t size);
  static int fileWrite(int handle, const void* buffer, size_t size);
  static bool fileDelete(const char* path);
  static bool fileExists(const char* path);
  static size_t fileSize(int handle);
  
  // Directory operations
  static int dirOpen(const char* path);
  static int dirClose(int handle);
  static bool dirRead(int handle, DirEntry* entry);
  static bool dirCreate(const char* path);
  static bool dirRemove(const char* path);
  static void dirRewind(int handle);
  
  // Memory operations
  static void* memAlloc(size_t size);
  static void memFree(void* ptr);
  static size_t memAvailable();
  static void memCompact();
  
  // IPC operations (NEW)
  static int ipcSend(int toTaskId, const void* data, size_t length);
  static int ipcReceive(void* buffer, size_t maxLength, int* fromTaskId = nullptr);
  static int ipcPoll();  // Returns number of messages waiting
  
  // Semaphore operations (NEW)
  static int semCreate(int initialValue, int maxValue, const char* name = nullptr);
  static int semWait(int semId, uint32_t timeoutMs = 0);
  static int semPost(int semId);
  static int semDestroy(int semId);
  
  // GPIO operations (NEW)
  static int gpioSetMode(int pin, int mode);
  static int gpioWrite(int pin, int value);
  static int gpioRead(int pin);
  static int gpioAnalogRead(int pin);
  static int gpioAnalogWrite(int pin, int value);
  
  // I2C operations (NEW)
  static int i2cBegin(uint8_t address = 0);
  static int i2cWrite(uint8_t address, const uint8_t* data, size_t length);
  static int i2cRead(uint8_t address, uint8_t* buffer, size_t length);
  static int i2cRequest(uint8_t address, size_t quantity);
  
  // SPI operations (NEW)
  static int spiBegin();
  static int spiTransfer(uint8_t* txData, uint8_t* rxData, size_t length);
  static int spiEnd();
  
  // Utility
  static void print(const char* message);
  static void debug(const char* message);
  static uint32_t uptime();
  static int getCurrentTaskId();
  static void printTaskList();
  static void printMemoryInfo();
};

// ============================================================================
// GLOBAL SYSCALL INTERFACE (for applications)
// ============================================================================

namespace OS {
  // File operations (backward compatible)
  inline int open(const char* path, bool write = false) {
    return Kernel::fileOpen(path, write);
  }
  
  inline int close(int fd) {
    return Kernel::fileClose(fd);
  }
  
  inline int read(int fd, void* buffer, size_t size) {
    return Kernel::fileRead(fd, buffer, size);
  }
  
  inline int write(int fd, const void* buffer, size_t size) {
    return Kernel::fileWrite(fd, buffer, size);
  }
  
  inline bool remove(const char* path) {
    return Kernel::fileDelete(path);
  }
  
  inline bool exists(const char* path) {
    return Kernel::fileExists(path);
  }
  
  inline size_t filesize(int fd) {
    return Kernel::fileSize(fd);
  }
  
  // Directory operations (backward compatible)
  inline int opendir(const char* path) {
    return Kernel::dirOpen(path);
  }
  
  inline int closedir(int dh) {
    return Kernel::dirClose(dh);
  }
  
  inline bool readdir(int dh, DirEntry* entry) {
    return Kernel::dirRead(dh, entry);
  }
  
  inline bool mkdir(const char* path) {
    return Kernel::dirCreate(path);
  }
  
  inline bool rmdir(const char* path) {
    return Kernel::dirRemove(path);
  }
  
  inline void rewinddir(int dh) {
    Kernel::dirRewind(dh);
  }
  
  // Memory operations (backward compatible)
  inline void* malloc(size_t size) {
    return Kernel::memAlloc(size);
  }
  
  inline void free(void* ptr) {
    Kernel::memFree(ptr);
  }
  
  inline void compact() {
    Kernel::memCompact();
  }
  
  // Task operations (backward compatible)
  inline void yield() {
    Kernel::yield();
  }
  
  inline void sleep(uint32_t ms) {
    Kernel::sleep(ms);
  }
  
  inline int getpid() {
    return Kernel::getCurrentTaskId();
  }
  
  // IPC operations (NEW)
  inline int send(int toTaskId, const void* data, size_t length) {
    return Kernel::ipcSend(toTaskId, data, length);
  }
  
  inline int receive(void* buffer, size_t maxLength, int* fromTaskId = nullptr) {
    return Kernel::ipcReceive(buffer, maxLength, fromTaskId);
  }
  
  inline int poll() {
    return Kernel::ipcPoll();
  }
  
  // Semaphore operations (NEW)
  inline int semCreate(int initialValue, int maxValue = 1, const char* name = nullptr) {
    return Kernel::semCreate(initialValue, maxValue, name);
  }
  
  inline int semWait(int semId, uint32_t timeoutMs = 0) {
    return Kernel::semWait(semId, timeoutMs);
  }
  
  inline int semPost(int semId) {
    return Kernel::semPost(semId);
  }
  
  inline int semDestroy(int semId) {
    return Kernel::semDestroy(semId);
  }
  
  // GPIO operations (NEW)
  inline int pinMode(int pin, int mode) {
    return Kernel::gpioSetMode(pin, mode);
  }
  
  inline int digitalWrite(int pin, int value) {
    return Kernel::gpioWrite(pin, value);
  }
  
  inline int digitalRead(int pin) {
    return Kernel::gpioRead(pin);
  }
  
  inline int analogRead(int pin) {
    return Kernel::gpioAnalogRead(pin);
  }
  
  inline int analogWrite(int pin, int value) {
    return Kernel::gpioAnalogWrite(pin, value);
  }
  
  // I2C operations (NEW)
  inline int i2cBegin(uint8_t address = 0) {
    return Kernel::i2cBegin(address);
  }
  
  inline int i2cWrite(uint8_t address, const uint8_t* data, size_t length) {
    return Kernel::i2cWrite(address, data, length);
  }
  
  inline int i2cRead(uint8_t address, uint8_t* buffer, size_t length) {
    return Kernel::i2cRead(address, buffer, length);
  }
  
  inline int i2cRequest(uint8_t address, size_t quantity) {
    return Kernel::i2cRequest(address, quantity);
  }
  
  // SPI operations (NEW)
  inline int spiBegin() {
    return Kernel::spiBegin();
  }
  
  inline int spiTransfer(uint8_t* txData, uint8_t* rxData, size_t length) {
    return Kernel::spiTransfer(txData, rxData, length);
  }
  
  inline int spiEnd() {
    return Kernel::spiEnd();
  }
  
  // System operations (backward compatible)
  inline void print(const char* message) {
    Kernel::print(message);
  }
  
  inline void debug(const char* message) {
    Kernel::debug(message);
  }
  
  inline uint32_t uptime() {
    return Kernel::uptime();
  }
}

#endif // KERNEL_H
