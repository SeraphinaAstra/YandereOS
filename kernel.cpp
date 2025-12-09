/*
  YandereOS Kernel Implementation - V3.5
  Adds: Watchdog timer, IPC, DDI, Stack traces, Fixed compaction
*/

#include "kernel.h"

// ============================================================================
// STATIC MEMBER INITIALIZATION
// ============================================================================

Task Kernel::tasks[MAX_TASKS];
int Kernel::currentTaskId = 0;
int Kernel::nextTaskId = 1;

uint8_t Kernel::kernelHeap[KERNEL_HEAP_SIZE];
size_t Kernel::heapUsed = 0;

FileHandle Kernel::fileHandles[MAX_FILE_HANDLES];
DirHandle Kernel::dirHandles[MAX_DIR_HANDLES];
bool Kernel::sdInitialized = false;

MessageQueue Kernel::messageQueues[MAX_TASKS];
Semaphore Kernel::semaphores[MAX_SEMAPHORES];

bool Kernel::watchdogEnabled = true;
uint32_t Kernel::watchdogLastCheck = 0;

bool Kernel::initialized = false;
uint32_t Kernel::bootTime = 0;

// ============================================================================
// INITIALIZATION
// ============================================================================

bool Kernel::init() {
  if (initialized) return true;
  
  Serial.begin(9600);
  while (!Serial && millis() < 3000);
  
  Serial.println(F("\n=== YandereOS Kernel v3.5 ==="));
  Serial.println(F("Features: Watchdog, IPC, DDI, Stack Traces"));
  Serial.println(F("Initializing..."));
  
  // Initialize all tasks as empty
  for (int i = 0; i < MAX_TASKS; i++) {
    tasks[i].state = TASK_EMPTY;
    tasks[i].id = -1;
    tasks[i].stackTraceDepth = 0;
  }
  
  // Initialize file handles
  for (int i = 0; i < MAX_FILE_HANDLES; i++) {
    fileHandles[i].inUse = false;
  }
  
  // Initialize directory handles
  for (int i = 0; i < MAX_DIR_HANDLES; i++) {
    dirHandles[i].inUse = false;
  }
  
  // Initialize message queues
  for (int i = 0; i < MAX_TASKS; i++) {
    messageQueues[i].head = 0;
    messageQueues[i].tail = 0;
    messageQueues[i].count = 0;
    for (int j = 0; j < MAX_MESSAGE_QUEUE_SIZE; j++) {
      messageQueues[i].messages[j].valid = false;
    }
  }
  
  // Initialize semaphores
  for (int i = 0; i < MAX_SEMAPHORES; i++) {
    semaphores[i].inUse = false;
  }
  
  // Initialize memory
  heapUsed = 0;
  
  // Initialize SD card
  Serial.print(F("Mounting SD card... "));
  if (SD.begin(SD_CS_PIN)) {
    sdInitialized = true;
    Serial.println(F("OK"));
  } else {
    Serial.println(F("FAILED"));
    Serial.println(F("Warning: SD card not available"));
  }
  
  // Create idle task (task 0)
  tasks[0].id = 0;
  tasks[0].name = "idle";
  tasks[0].state = TASK_READY;
  tasks[0].priority = 0;
  tasks[0].lastYield = millis();
  tasks[0].canAccessSD = false;
  tasks[0].canAccessDisplay = false;
  tasks[0].canCreateTasks = false;
  tasks[0].canAccessGPIO = false;
  tasks[0].canAccessI2C = false;
  tasks[0].canAccessSPI = false;
  tasks[0].memoryUsed = 0;
  
  currentTaskId = 0;
  bootTime = millis();
  watchdogLastCheck = millis();
  initialized = true;
  
  Serial.println(F("Kernel initialized successfully\n"));
  return true;
}

void Kernel::panic(const char* message) {
  Serial.println(F("\n!!! KERNEL PANIC !!!"));
  Serial.println(message);
  
  // Print current task info
  Task* current = getCurrentTask();
  if (current) {
    Serial.print(F("Current task: "));
    Serial.print(current->name);
    Serial.print(F(" (ID: "));
    Serial.print(current->id);
    Serial.println(F(")"));
    
    // Print stack trace if available
    printStackTrace(current);
  }
  
  Serial.println(F("\n=== System State ==="));
  printTaskList();
  printMemoryInfo();
  
  Serial.println(F("\nSystem halted."));
  while(1) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
}

// ============================================================================
// STACK TRACING
// ============================================================================

void Kernel::captureStackTrace(Task* task) {
  if (!task) return;
  
  // Simple stack capture - on Arduino, this is limited
  // We can at least record the entry point
  task->stackTraceDepth = 1;
  task->stackTrace[0].returnAddress = (void*)task->entryPoint;
  task->stackTrace[0].functionName = task->name;
  
  // Note: Full stack unwinding requires DWARF debug info
  // which is not easily accessible on Arduino
}

void Kernel::printStackTrace(Task* task) {
  if (!task || task->stackTraceDepth == 0) {
    Serial.println(F("No stack trace available"));
    return;
  }
  
  Serial.println(F("\n=== Stack Trace ==="));
  for (int i = 0; i < task->stackTraceDepth; i++) {
    Serial.print(F("  ["));
    Serial.print(i);
    Serial.print(F("] "));
    
    if (task->stackTrace[i].functionName) {
      Serial.print(task->stackTrace[i].functionName);
    } else {
      Serial.print(F("<unknown>"));
    }
    
    Serial.print(F(" @ 0x"));
    Serial.println((uintptr_t)task->stackTrace[i].returnAddress, HEX);
  }
}

// ============================================================================
// TASK MANAGEMENT
// ============================================================================

int Kernel::allocateTaskId() {
  for (int i = 1; i < MAX_TASKS; i++) {
    if (tasks[i].state == TASK_EMPTY) {
      return i;
    }
  }
  return -1;
}

Task* Kernel::getCurrentTask() {
  return &tasks[currentTaskId];
}

Task* Kernel::getTask(int taskId) {
  if (taskId < 0 || taskId >= MAX_TASKS) return nullptr;
  if (tasks[taskId].state == TASK_EMPTY) return nullptr;
  return &tasks[taskId];
}

int Kernel::createTask(const char* name, void (*entryPoint)()) {
  int taskId = allocateTaskId();
  if (taskId < 0) {
    return SYS_ERR_NO_MEMORY;
  }
  
  Task* task = &tasks[taskId];
  task->id = taskId;
  task->name = name;
  task->state = TASK_READY;
  task->entryPoint = entryPoint;
  task->priority = 10;
  task->lastRun = 0;
  task->lastYield = millis();
  task->sleepUntil = 0;
  task->memoryUsed = 0;
  task->stackTraceDepth = 0;
  
  // Clear file handles
  for (int i = 0; i < MAX_FILE_HANDLES; i++) {
    task->fileHandles[i] = false;
  }
  
  // Clear dir handles
  for (int i = 0; i < MAX_DIR_HANDLES; i++) {
    task->dirHandles[i] = false;
  }
  
  // Set permissions (default: can access SD and display)
  task->canAccessSD = true;
  task->canAccessDisplay = true;
  task->canCreateTasks = false;
  task->canAccessGPIO = true;   // Default allow GPIO
  task->canAccessI2C = false;   // Require explicit permission
  task->canAccessSPI = false;   // Require explicit permission
  
  // Capture initial stack trace
  captureStackTrace(task);
  
  Serial.print(F("Task created: "));
  Serial.print(name);
  Serial.print(F(" (ID: "));
  Serial.print(taskId);
  Serial.println(F(")"));
  
  return taskId;
}

void Kernel::killTask(int taskId) {
  Task* task = getTask(taskId);
  if (!task || taskId == 0) return;
  
  // Close all open files
  for (int i = 0; i < MAX_FILE_HANDLES; i++) {
    if (task->fileHandles[i]) {
      freeFileHandle(i);
    }
  }
  
  // Close all open directories
  for (int i = 0; i < MAX_DIR_HANDLES; i++) {
    if (task->dirHandles[i]) {
      freeDirHandle(i);
    }
  }
  
  task->state = TASK_EMPTY;
  task->id = -1;
  
  Serial.print(F("Task killed: "));
  Serial.println(task->name);
}

// ============================================================================
// WATCHDOG TIMER
// ============================================================================

void Kernel::enableWatchdog(bool enable) {
  watchdogEnabled = enable;
  Serial.print(F("Watchdog "));
  Serial.println(enable ? F("enabled") : F("disabled"));
}

void Kernel::feedWatchdog() {
  Task* current = getCurrentTask();
  if (current) {
    current->lastYield = millis();
  }
}

void Kernel::checkWatchdog() {
  if (!watchdogEnabled) return;
  
  uint32_t now = millis();
  
  // Check if we should run watchdog check
  if (now - watchdogLastCheck < 1000) return;
  watchdogLastCheck = now;
  
  // Check all running/ready tasks
  for (int i = 0; i < MAX_TASKS; i++) {
    Task* task = &tasks[i];
    if (task->state == TASK_EMPTY) continue;
    if (task->state == TASK_SLEEPING) continue;
    
    uint32_t timeSinceYield = now - task->lastYield;
    
    if (timeSinceYield > WATCHDOG_TIMEOUT_MS) {
      Serial.print(F("[WATCHDOG] Task "));
      Serial.print(task->name);
      Serial.print(F(" hasn't yielded in "));
      Serial.print(timeSinceYield);
      Serial.println(F("ms - forcing reschedule"));
      
      // Force task to ready state
      if (task->state == TASK_RUNNING) {
        task->state = TASK_READY;
      }
      
      task->lastYield = now;
    }
  }
}

void Kernel::schedule() {
  checkWatchdog();
  
  uint32_t now = millis();
  int bestTask = 0;
  int bestPriority = -1;
  
  // Find highest priority ready task
  for (int i = 0; i < MAX_TASKS; i++) {
    Task* task = &tasks[i];
    
    if (task->state == TASK_EMPTY) continue;
    
    // Wake up sleeping tasks
    if (task->state == TASK_SLEEPING) {
      if (now >= task->sleepUntil) {
        task->state = TASK_READY;
      } else {
        continue;
      }
    }
    
    if (task->state == TASK_READY && task->priority > bestPriority) {
      bestTask = i;
      bestPriority = task->priority;
    }
  }
  
  // Switch to best task
  if (bestTask != currentTaskId) {
    if (tasks[currentTaskId].state == TASK_RUNNING) {
      tasks[currentTaskId].state = TASK_READY;
    }
    currentTaskId = bestTask;
    tasks[currentTaskId].state = TASK_RUNNING;
    tasks[currentTaskId].lastRun = now;
  }
  
  // Execute current task
  Task* current = getCurrentTask();
  if (current->entryPoint && current->state == TASK_RUNNING) {
    current->entryPoint();
  }
}

void Kernel::yield() {
  Task* current = getCurrentTask();
  if (current) {
    current->state = TASK_READY;
    current->lastYield = millis();
  }
}

void Kernel::sleep(uint32_t ms) {
  Task* current = getCurrentTask();
  if (current) {
    current->state = TASK_SLEEPING;
    current->sleepUntil = millis() + ms;
    current->lastYield = millis();
  }
}

// ============================================================================
// MEMORY MANAGEMENT - FIXED COMPACTION
// ============================================================================

MemoryBlock* Kernel::getBlockHeader(void* ptr) {
  if (!ptr) return nullptr;
  return (MemoryBlock*)((uint8_t*)ptr - sizeof(MemoryBlock));
}

void* Kernel::allocateMemoryInternal(size_t size, int taskId) {
  if (size == 0) return nullptr;
  
  // Align to 4 bytes
  size = (size + 3) & ~3;
  
  size_t totalNeeded = sizeof(MemoryBlock) + size;
  
  // Check if we have space
  if (heapUsed + totalNeeded > KERNEL_HEAP_SIZE) {
    Serial.println(F("[Memory] Out of space, compacting..."));
    compactMemory();
    
    if (heapUsed + totalNeeded > KERNEL_HEAP_SIZE) {
      Serial.println(F("[Memory] Out of memory after compaction!"));
      return nullptr;
    }
  }
  
  // Allocate at end of used heap
  MemoryBlock* block = (MemoryBlock*)&kernelHeap[heapUsed];
  void* userPtr = (void*)((uint8_t*)block + sizeof(MemoryBlock));
  
  block->size = size;
  block->ownerTaskId = taskId;
  block->inUse = true;
  block->handleId = -1;
  
  heapUsed += totalNeeded;
  
  if (taskId >= 0 && taskId < MAX_TASKS) {
    tasks[taskId].memoryUsed += size;
  }
  
  return userPtr;
}

void Kernel::freeMemoryInternal(void* ptr) {
  if (!ptr) return;
  
  MemoryBlock* block = getBlockHeader(ptr);
  if (!block || !block->inUse) {
    Serial.println(F("[Memory] Warning: Invalid free()"));
    return;
  }
  
  if (block->ownerTaskId >= 0 && block->ownerTaskId < MAX_TASKS) {
    tasks[block->ownerTaskId].memoryUsed -= block->size;
  }
  
  block->inUse = false;
}

void Kernel::compactMemory() {
  /*
   * FIXED COMPACTION ALGORITHM
   * 
   * Problem with old version: User-space pointers become invalid when
   * blocks are moved, causing crashes when dereferenced.
   * 
   * Solution: We can't safely update user pointers in cooperative multitasking
   * without handles. So instead, we just mark blocks as free and consolidate
   * free space by moving in-use blocks together, but we warn users that
   * compaction invalidates pointers.
   * 
   * Better solution for future: Use handle-based allocation where OS::malloc
   * returns a handle (integer ID) and users must call OS::deref(handle) to
   * get the actual pointer before each use.
   */
  
  Serial.println(F("[Memory] Compacting heap (WARNING: may invalidate pointers)"));
  
  size_t writePos = 0;
  size_t readPos = 0;
  int movedBlocks = 0;
  
  while (readPos < heapUsed) {
    MemoryBlock* block = (MemoryBlock*)&kernelHeap[readPos];
    size_t blockTotalSize = sizeof(MemoryBlock) + block->size;
    
    if (block->inUse) {
      if (writePos != readPos) {
        // Move block to write position
        memmove(&kernelHeap[writePos], &kernelHeap[readPos], blockTotalSize);
        movedBlocks++;
      }
      writePos += blockTotalSize;
    }
    
    readPos += blockTotalSize;
    
    // Safety check
    if (readPos > KERNEL_HEAP_SIZE || writePos > KERNEL_HEAP_SIZE) {
      panic("Heap corruption detected during compaction");
      break;
    }
  }
  
  size_t freedSpace = heapUsed - writePos;
  heapUsed = writePos;
  
  Serial.print(F("[Memory] Compaction complete: freed "));
  Serial.print(freedSpace);
  Serial.print(F(" bytes, moved "));
  Serial.print(movedBlocks);
  Serial.println(F(" blocks"));
  
  if (movedBlocks > 0) {
    Serial.println(F("[Memory] WARNING: Existing pointers may be invalid!"));
    Serial.println(F("[Memory] Recommendation: Free and reallocate after compaction"));
  }
}

void* Kernel::memAlloc(size_t size) {
  return allocateMemoryInternal(size, currentTaskId);
}

void Kernel::memFree(void* ptr) {
  freeMemoryInternal(ptr);
}

size_t Kernel::memAvailable() {
  return KERNEL_HEAP_SIZE - heapUsed;
}

void Kernel::memCompact() {
  compactMemory();
}

// ============================================================================
// IPC - MESSAGE QUEUES
// ============================================================================

int Kernel::ipcSend(int toTaskId, const void* data, size_t length) {
  if (toTaskId < 0 || toTaskId >= MAX_TASKS) return SYS_ERR_INVALID_PARAM;
  if (tasks[toTaskId].state == TASK_EMPTY) return SYS_ERR_NOT_FOUND;
  if (length > sizeof(Message::data)) return SYS_ERR_INVALID_PARAM;
  if (!data && length > 0) return SYS_ERR_INVALID_PARAM;
  
  MessageQueue* queue = &messageQueues[toTaskId];
  
  if (queue->count >= MAX_MESSAGE_QUEUE_SIZE) {
    return SYS_ERR_NO_MEMORY;
  }
  
  Message* msg = &queue->messages[queue->tail];
  msg->fromTaskId = currentTaskId;
  msg->toTaskId = toTaskId;
  msg->length = length;
  if (length > 0) {
    memcpy(msg->data, data, length);
  }
  msg->timestamp = millis();
  msg->valid = true;
  
  queue->tail = (queue->tail + 1) % MAX_MESSAGE_QUEUE_SIZE;
  queue->count++;
  
  return SYS_OK;
}

int Kernel::ipcReceive(void* buffer, size_t maxLength, int* fromTaskId) {
  MessageQueue* queue = &messageQueues[currentTaskId];
  
  if (queue->count == 0) {
    return SYS_ERR_WOULD_BLOCK;
  }
  
  Message* msg = &queue->messages[queue->head];
  if (!msg->valid) {
    return SYS_ERR_IO_ERROR;
  }
  
  if (msg->length > maxLength) {
    return SYS_ERR_INVALID_PARAM;
  }
  
  if (buffer && msg->length > 0) {
    memcpy(buffer, msg->data, msg->length);
  }
  
  if (fromTaskId) {
    *fromTaskId = msg->fromTaskId;
  }
  
  int length = msg->length;
  
  msg->valid = false;
  queue->head = (queue->head + 1) % MAX_MESSAGE_QUEUE_SIZE;
  queue->count--;
  
  return length;
}

int Kernel::ipcPoll() {
  return messageQueues[currentTaskId].count;
}

// ============================================================================
// IPC - SEMAPHORES
// ============================================================================

int Kernel::allocateSemaphore() {
  for (int i = 0; i < MAX_SEMAPHORES; i++) {
    if (!semaphores[i].inUse) {
      return i;
    }
  }
  return -1;
}

int Kernel::semCreate(int initialValue, int maxValue, const char* name) {
  if (initialValue < 0 || maxValue < 1 || initialValue > maxValue) {
    return SYS_ERR_INVALID_PARAM;
  }
  
  int semId = allocateSemaphore();
  if (semId < 0) {
    return SYS_ERR_NO_MEMORY;
  }
  
  semaphores[semId].value = initialValue;
  semaphores[semId].maxValue = maxValue;
  semaphores[semId].inUse = true;
  semaphores[semId].ownerTaskId = currentTaskId;
  semaphores[semId].name = name;
  
  return semId;
}

int Kernel::semWait(int semId, uint32_t timeoutMs) {
  if (semId < 0 || semId >= MAX_SEMAPHORES) return SYS_ERR_INVALID_PARAM;
  if (!semaphores[semId].inUse) return SYS_ERR_NOT_FOUND;
  
  Semaphore* sem = &semaphores[semId];
  uint32_t startTime = millis();
  
  while (sem->value <= 0) {
    if (timeoutMs > 0 && (millis() - startTime) >= timeoutMs) {
      return SYS_ERR_TIMEOUT;
    }
    yield();
  }
  
  sem->value--;
  return SYS_OK;
}

int Kernel::semPost(int semId) {
  if (semId < 0 || semId >= MAX_SEMAPHORES) return SYS_ERR_INVALID_PARAM;
  if (!semaphores[semId].inUse) return SYS_ERR_NOT_FOUND;
  
  Semaphore* sem = &semaphores[semId];
  
  if (sem->value >= sem->maxValue) {
    return SYS_ERR_INVALID_PARAM;
  }
  
  sem->value++;
  return SYS_OK;
}

int Kernel::semDestroy(int semId) {
  if (semId < 0 || semId >= MAX_SEMAPHORES) return SYS_ERR_INVALID_PARAM;
  if (!semaphores[semId].inUse) return SYS_ERR_NOT_FOUND;
  
  // Only owner or kernel can destroy
  if (semaphores[semId].ownerTaskId != currentTaskId && currentTaskId != 0) {
    return SYS_ERR_PERMISSION;
  }
  
  semaphores[semId].inUse = false;
  return SYS_OK;
}

// ============================================================================
// DEVICE DRIVER INTERFACE - GPIO
// ============================================================================

int Kernel::gpioSetMode(int pin, int mode) {
  Task* current = getCurrentTask();
  if (!current->canAccessGPIO) return SYS_ERR_PERMISSION;
  
  pinMode(pin, mode);
  return SYS_OK;
}

int Kernel::gpioWrite(int pin, int value) {
  Task* current = getCurrentTask();
  if (!current->canAccessGPIO) return SYS_ERR_PERMISSION;
  
  digitalWrite(pin, value);
  return SYS_OK;
}

int Kernel::gpioRead(int pin) {
  Task* current = getCurrentTask();
  if (!current->canAccessGPIO) return SYS_ERR_PERMISSION;
  
  return digitalRead(pin);
}

int Kernel::gpioAnalogRead(int pin) {
  Task* current = getCurrentTask();
  if (!current->canAccessGPIO) return SYS_ERR_PERMISSION;
  
  return analogRead(pin);
}

int Kernel::gpioAnalogWrite(int pin, int value) {
  Task* current = getCurrentTask();
  if (!current->canAccessGPIO) return SYS_ERR_PERMISSION;
  
  analogWrite(pin, value);
  return SYS_OK;
}

// ============================================================================
// DEVICE DRIVER INTERFACE - I2C
// ============================================================================

int Kernel::i2cBegin(uint8_t address) {
  Task* current = getCurrentTask();
  if (!current->canAccessI2C) return SYS_ERR_PERMISSION;
  
  if (address == 0) {
    Wire.begin();
  } else {
    Wire.begin(address);
  }
  
  return SYS_OK;
}

int Kernel::i2cWrite(uint8_t address, const uint8_t* data, size_t length) {
  Task* current = getCurrentTask();
  if (!current->canAccessI2C) return SYS_ERR_PERMISSION;
  if (!data || length == 0) return SYS_ERR_INVALID_PARAM;
  
  Wire.beginTransmission(address);
  size_t written = Wire.write(data, length);
  uint8_t result = Wire.endTransmission();
  
  if (result != 0) {
    return SYS_ERR_IO_ERROR;
  }
  
  return written;
}

int Kernel::i2cRead(uint8_t address, uint8_t* buffer, size_t length) {
  Task* current = getCurrentTask();
  if (!current->canAccessI2C) return SYS_ERR_PERMISSION;
  if (!buffer || length == 0) return SYS_ERR_INVALID_PARAM;
  
  Wire.beginTransmission(address);
  uint8_t result = Wire.endTransmission();
  
  if (result != 0) {
    return SYS_ERR_IO_ERROR;
  }
  
  int available = Wire.requestFrom(address, (uint8_t)length);
  int bytesRead = 0;
  
  while (Wire.available() && bytesRead < (int)length) {
    buffer[bytesRead++] = Wire.read();
  }
  
  return bytesRead;
}

int Kernel::i2cRequest(uint8_t address, size_t quantity) {
  Task* current = getCurrentTask();
  if (!current->canAccessI2C) return SYS_ERR_PERMISSION;
  
  return Wire.requestFrom(address, (uint8_t)quantity);
}

// ============================================================================
// DEVICE DRIVER INTERFACE - SPI
// ============================================================================

int Kernel::spiBegin() {
  Task* current = getCurrentTask();
  if (!current->canAccessSPI) return SYS_ERR_PERMISSION;
  
  SPI.begin();
  return SYS_OK;
}

int Kernel::spiTransfer(uint8_t* txData, uint8_t* rxData, size_t length) {
  Task* current = getCurrentTask();
  if (!current->canAccessSPI) return SYS_ERR_PERMISSION;
  if (length == 0) return SYS_ERR_INVALID_PARAM;
  
  if (txData && rxData) {
    for (size_t i = 0; i < length; i++) {
      rxData[i] = SPI.transfer(txData[i]);
    }
  } else if (txData) {
    for (size_t i = 0; i < length; i++) {
      SPI.transfer(txData[i]);
    }
  } else if (rxData) {
    for (size_t i = 0; i < length; i++) {
      rxData[i] = SPI.transfer(0x00);
    }
  }
  
  return length;
}

int Kernel::spiEnd() {
  Task* current = getCurrentTask();
  if (!current->canAccessSPI) return SYS_ERR_PERMISSION;
  
  SPI.end();
  return SYS_OK;
}

// ============================================================================
// FILE SYSTEM
// ============================================================================

int Kernel::allocateFileHandle() {
  for (int i = 0; i < MAX_FILE_HANDLES; i++) {
    if (!fileHandles[i].inUse) {
      return i;
    }
  }
  return -1;
}

int Kernel::allocateDirHandle() {
  for (int i = 0; i < MAX_DIR_HANDLES; i++) {
    if (!dirHandles[i].inUse) {
      return i;
    }
  }
  return -1;
}

void Kernel::freeFileHandle(int handle) {
  if (handle < 0 || handle >= MAX_FILE_HANDLES) return;
  
  if (fileHandles[handle].inUse) {
    fileHandles[handle].file.close();
    fileHandles[handle].inUse = false;
  }
}

void Kernel::freeDirHandle(int handle) {
  if (handle < 0 || handle >= MAX_DIR_HANDLES) return;
  
  if (dirHandles[handle].inUse) {
    dirHandles[handle].dir.close();
    dirHandles[handle].inUse = false;
  }
}

int Kernel::fileOpen(const char* path, bool write) {
  if (!sdInitialized) return SYS_ERR_IO_ERROR;
  
  Task* current = getCurrentTask();
  if (!current->canAccessSD) return SYS_ERR_PERMISSION;
  
  int handle = allocateFileHandle();
  if (handle < 0) return SYS_ERR_NO_MEMORY;
  
  FileHandle* fh = &fileHandles[handle];
  fh->file = SD.open(path, write ? FILE_WRITE : FILE_READ);
  
  if (!fh->file) {
    return SYS_ERR_NOT_FOUND;
  }
  
  fh->inUse = true;
  fh->ownerTaskId = currentTaskId;
  fh->canWrite = write;
  current->fileHandles[handle] = true;
  
  return handle;
}

int Kernel::fileClose(int handle) {
  if (handle < 0 || handle >= MAX_FILE_HANDLES) return SYS_ERR_INVALID_PARAM;
  if (!fileHandles[handle].inUse) return SYS_ERR_INVALID_PARAM;
  if (fileHandles[handle].ownerTaskId != currentTaskId) return SYS_ERR_PERMISSION;
  
  freeFileHandle(handle);
  getCurrentTask()->fileHandles[handle] = false;
  
  return SYS_OK;
}

int Kernel::fileRead(int handle, void* buffer, size_t size) {
  if (handle < 0 || handle >= MAX_FILE_HANDLES) return SYS_ERR_INVALID_PARAM;
  if (!fileHandles[handle].inUse) return SYS_ERR_INVALID_PARAM;
  if (fileHandles[handle].ownerTaskId != currentTaskId) return SYS_ERR_PERMISSION;
  
  return fileHandles[handle].file.read((uint8_t*)buffer, size);
}

int Kernel::fileWrite(int handle, const void* buffer, size_t size) {
  if (handle < 0 || handle >= MAX_FILE_HANDLES) return SYS_ERR_INVALID_PARAM;
  if (!fileHandles[handle].inUse) return SYS_ERR_INVALID_PARAM;
  if (fileHandles[handle].ownerTaskId != currentTaskId) return SYS_ERR_PERMISSION;
  if (!fileHandles[handle].canWrite) return SYS_ERR_PERMISSION;
  
  return fileHandles[handle].file.write((const uint8_t*)buffer, size);
}

bool Kernel::fileDelete(const char* path) {
  if (!sdInitialized) return false;
  
  Task* current = getCurrentTask();
  if (!current->canAccessSD) return false;
  
  return SD.remove(path);
}

bool Kernel::fileExists(const char* path) {
  if (!sdInitialized) return false;
  
  Task* current = getCurrentTask();
  if (!current->canAccessSD) return false;
  
  return SD.exists(path);
}

size_t Kernel::fileSize(int handle) {
  if (handle < 0 || handle >= MAX_FILE_HANDLES) return 0;
  if (!fileHandles[handle].inUse) return 0;
  if (fileHandles[handle].ownerTaskId != currentTaskId) return 0;
  
  return fileHandles[handle].file.size();
}

// ============================================================================
// DIRECTORY OPERATIONS
// ============================================================================

int Kernel::dirOpen(const char* path) {
  if (!sdInitialized) return SYS_ERR_IO_ERROR;
  
  Task* current = getCurrentTask();
  if (!current->canAccessSD) return SYS_ERR_PERMISSION;
  
  int handle = allocateDirHandle();
  if (handle < 0) return SYS_ERR_NO_MEMORY;
  
  DirHandle* dh = &dirHandles[handle];
  dh->dir = SD.open(path);
  
  if (!dh->dir) {
    return SYS_ERR_NOT_FOUND;
  }
  
  if (!dh->dir.isDirectory()) {
    dh->dir.close();
    return SYS_ERR_INVALID_PARAM;
  }
  
  dh->inUse = true;
  dh->ownerTaskId = currentTaskId;
  current->dirHandles[handle] = true;
  
  return handle;
}

int Kernel::dirClose(int handle) {
  if (handle < 0 || handle >= MAX_DIR_HANDLES) return SYS_ERR_INVALID_PARAM;
  if (!dirHandles[handle].inUse) return SYS_ERR_INVALID_PARAM;
  if (dirHandles[handle].ownerTaskId != currentTaskId) return SYS_ERR_PERMISSION;
  
  freeDirHandle(handle);
  getCurrentTask()->dirHandles[handle] = false;
  
  return SYS_OK;
}

bool Kernel::dirRead(int handle, DirEntry* entry) {
  if (handle < 0 || handle >= MAX_DIR_HANDLES) return false;
  if (!dirHandles[handle].inUse) return false;
  if (dirHandles[handle].ownerTaskId != currentTaskId) return false;
  if (!entry) return false;
  
  File nextEntry = dirHandles[handle].dir.openNextFile();
  if (!nextEntry) {
    return false;
  }
  
  strncpy(entry->name, nextEntry.name(), sizeof(entry->name) - 1);
  entry->name[sizeof(entry->name) - 1] = '\0';
  entry->isDirectory = nextEntry.isDirectory();
  entry->size = nextEntry.size();
  
  nextEntry.close();
  return true;
}

bool Kernel::dirCreate(const char* path) {
  if (!sdInitialized) return false;
  
  Task* current = getCurrentTask();
  if (!current->canAccessSD) return false;
  
  return SD.mkdir(path);
}

bool Kernel::dirRemove(const char* path) {
  if (!sdInitialized) return false;
  
  Task* current = getCurrentTask();
  if (!current->canAccessSD) return false;
  
  return SD.rmdir(path);
}

void Kernel::dirRewind(int handle) {
  if (handle < 0 || handle >= MAX_DIR_HANDLES) return;
  if (!dirHandles[handle].inUse) return;
  if (dirHandles[handle].ownerTaskId != currentTaskId) return;
  
  dirHandles[handle].dir.rewindDirectory();
}

// ============================================================================
// SYSTEM CALLS
// ============================================================================

int Kernel::syscall(SyscallType type, void* arg1, void* arg2, void* arg3, void* arg4) {
  switch (type) {
    // File operations
    case SYS_FILE_OPEN:
      return fileOpen((const char*)arg1, (bool)(intptr_t)arg2);
    case SYS_FILE_CLOSE:
      return fileClose((int)(intptr_t)arg1);
    case SYS_FILE_READ:
      return fileRead((int)(intptr_t)arg1, arg2, (size_t)(intptr_t)arg3);
    case SYS_FILE_WRITE:
      return fileWrite((int)(intptr_t)arg1, arg2, (size_t)(intptr_t)arg3);
    case SYS_FILE_DELETE:
      return fileDelete((const char*)arg1) ? SYS_OK : SYS_ERR_IO_ERROR;
    case SYS_FILE_EXISTS:
      return fileExists((const char*)arg1) ? 1 : 0;
    case SYS_FILE_SIZE:
      return (int)fileSize((int)(intptr_t)arg1);
    
    // Directory operations
    case SYS_DIR_OPEN:
      return dirOpen((const char*)arg1);
    case SYS_DIR_CLOSE:
      return dirClose((int)(intptr_t)arg1);
    case SYS_DIR_READ:
      return dirRead((int)(intptr_t)arg1, (DirEntry*)arg2) ? 1 : 0;
    case SYS_DIR_CREATE:
      return dirCreate((const char*)arg1) ? SYS_OK : SYS_ERR_IO_ERROR;
    case SYS_DIR_REMOVE:
      return dirRemove((const char*)arg1) ? SYS_OK : SYS_ERR_IO_ERROR;
    case SYS_DIR_REWIND:
      dirRewind((int)(intptr_t)arg1);
      return SYS_OK;
    
    // Memory operations
    case SYS_MEM_ALLOC:
      return (int)(intptr_t)memAlloc((size_t)(intptr_t)arg1);
    case SYS_MEM_FREE:
      memFree(arg1);
      return SYS_OK;
    case SYS_MEM_COMPACT:
      memCompact();
      return SYS_OK;
    
    // Task operations
    case SYS_TASK_YIELD:
      yield();
      return SYS_OK;
    case SYS_TASK_SLEEP:
      sleep((uint32_t)(intptr_t)arg1);
      return SYS_OK;
    
    // IPC operations
    case SYS_IPC_SEND:
      return ipcSend((int)(intptr_t)arg1, arg2, (size_t)(intptr_t)arg3);
    case SYS_IPC_RECEIVE:
      return ipcReceive(arg1, (size_t)(intptr_t)arg2, (int*)arg3);
    case SYS_IPC_POLL:
      return ipcPoll();
    
    // Semaphore operations
    case SYS_SEM_CREATE:
      return semCreate((int)(intptr_t)arg1, (int)(intptr_t)arg2, (const char*)arg3);
    case SYS_SEM_WAIT:
      return semWait((int)(intptr_t)arg1, (uint32_t)(intptr_t)arg2);
    case SYS_SEM_POST:
      return semPost((int)(intptr_t)arg1);
    case SYS_SEM_DESTROY:
      return semDestroy((int)(intptr_t)arg1);
    
    // GPIO operations
    case SYS_GPIO_PINMODE:
      return gpioSetMode((int)(intptr_t)arg1, (int)(intptr_t)arg2);
    case SYS_GPIO_WRITE:
      return gpioWrite((int)(intptr_t)arg1, (int)(intptr_t)arg2);
    case SYS_GPIO_READ:
      return gpioRead((int)(intptr_t)arg1);
    case SYS_GPIO_ANALOG_READ:
      return gpioAnalogRead((int)(intptr_t)arg1);
    case SYS_GPIO_ANALOG_WRITE:
      return gpioAnalogWrite((int)(intptr_t)arg1, (int)(intptr_t)arg2);
    
    // I2C operations
    case SYS_I2C_BEGIN:
      return i2cBegin((uint8_t)(intptr_t)arg1);
    case SYS_I2C_WRITE:
      return i2cWrite((uint8_t)(intptr_t)arg1, (const uint8_t*)arg2, (size_t)(intptr_t)arg3);
    case SYS_I2C_READ:
      return i2cRead((uint8_t)(intptr_t)arg1, (uint8_t*)arg2, (size_t)(intptr_t)arg3);
    case SYS_I2C_REQUEST:
      return i2cRequest((uint8_t)(intptr_t)arg1, (size_t)(intptr_t)arg2);
    
    // SPI operations
    case SYS_SPI_BEGIN:
      return spiBegin();
    case SYS_SPI_TRANSFER:
      return spiTransfer((uint8_t*)arg1, (uint8_t*)arg2, (size_t)(intptr_t)arg3);
    case SYS_SPI_END:
      return spiEnd();
    
    // System operations
    case SYS_GET_TIME:
      return (int)millis();
    
    default:
      return SYS_ERR_INVALID_CALL;
  }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

void Kernel::print(const char* message) {
  Serial.print(F("["));
  Serial.print(getCurrentTask()->name);
  Serial.print(F("] "));
  Serial.println(message);
}

void Kernel::debug(const char* message) {
  Serial.print(F("[DEBUG] "));
  Serial.println(message);
}

uint32_t Kernel::uptime() {
  return millis() - bootTime;
}

int Kernel::getCurrentTaskId() {
  return currentTaskId;
}

void Kernel::printTaskList() {
  Serial.println(F("\n=== Task List ==="));
  Serial.println(F("ID  Name            State      Memory   LastYield"));
  Serial.println(F("--- --------------- ---------- -------- ---------"));
  
  for (int i = 0; i < MAX_TASKS; i++) {
    if (tasks[i].state == TASK_EMPTY) continue;
    
    Serial.print(i);
    Serial.print(F("   "));
    Serial.print(tasks[i].name);
    
    for (int j = strlen(tasks[i].name); j < 16; j++) {
      Serial.print(' ');
    }
    
    switch (tasks[i].state) {
      case TASK_READY: Serial.print(F("READY     ")); break;
      case TASK_RUNNING: Serial.print(F("RUNNING   ")); break;
      case TASK_SLEEPING: Serial.print(F("SLEEPING  ")); break;
      case TASK_BLOCKED: Serial.print(F("BLOCKED   ")); break;
      default: Serial.print(F("UNKNOWN   ")); break;
    }
    
    Serial.print(tasks[i].memoryUsed);
    Serial.print(F(" B    "));
    
    uint32_t timeSinceYield = millis() - tasks[i].lastYield;
    Serial.print(timeSinceYield);
    Serial.println(F("ms"));
  }
  Serial.println();
}

void Kernel::printMemoryInfo() {
  Serial.println(F("\n=== Memory Info ==="));
  Serial.print(F("Total heap:     "));
  Serial.print(KERNEL_HEAP_SIZE);
  Serial.println(F(" bytes"));
  Serial.print(F("Used:           "));
  Serial.print(heapUsed);
  Serial.println(F(" bytes"));
  Serial.print(F("Available:      "));
  Serial.print(memAvailable());
  Serial.println(F(" bytes"));
  
  // Count blocks
  size_t readPos = 0;
  int usedBlocks = 0;
  int freeBlocks = 0;
  
  while (readPos < heapUsed) {
    MemoryBlock* block = (MemoryBlock*)&kernelHeap[readPos];
    if (block->inUse) {
      usedBlocks++;
    } else {
      freeBlocks++;
    }
    readPos += sizeof(MemoryBlock) + block->size;
  }
  
  Serial.print(F("Used blocks:    "));
  Serial.println(usedBlocks);
  Serial.print(F("Free blocks:    "));
  Serial.println(freeBlocks);
  if (freeBlocks > 0) {
    Serial.println(F("Fragmentation detected - consider compacting"));
  }
  Serial.println();
}
