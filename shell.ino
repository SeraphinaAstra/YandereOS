/*
  Shell Application - Full Featured with Directory Support
*/

#include "kernel.h"

// Shell state structure
struct ShellState {
  char currentDir[128];
  char commandBuffer[256];
  int cmdLen;
};

void shellTask() {
  // Allocate shell state through kernel heap
  ShellState* state = (ShellState*)OS::malloc(sizeof(ShellState));
  if (!state) {
    OS::print("FATAL: Cannot allocate shell state");
    return;
  }
  
  // Initialize state
  strcpy(state->currentDir, "/");
  state->commandBuffer[0] = '\0';
  state->cmdLen = 0;
  
  // Print initial prompt
  printPrompt(state->currentDir);
  
  // This function should never return - it's a continuous loop
  while (true) {
    while (Serial.available() > 0) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        if (state->cmdLen > 0) {
          Serial.println();
          state->commandBuffer[state->cmdLen] = '\0';
          processCommand(state->commandBuffer, state->currentDir);
          state->cmdLen = 0;
          state->commandBuffer[0] = '\0';
          printPrompt(state->currentDir);
        }
      } else if (c == 127 || c == 8) { // Backspace
        if (state->cmdLen > 0) {
          state->cmdLen--;
          Serial.write(8);
          Serial.write(' ');
          Serial.write(8);
        }
      } else if (state->cmdLen < 255) {
        state->commandBuffer[state->cmdLen++] = c;
        Serial.write(c);
      }
    }
    
    OS::yield(); // Give other tasks a chance
  }
  
  // Never reached, but good practice
  OS::free(state);
}

void printPrompt(const char* currentDir) {
  Serial.print(F("shell:"));
  Serial.print(currentDir);
  Serial.print(F("$ "));
}

void resolvePath(const char* path, const char* currentDir, char* output, size_t outputSize) {
  if (path[0] == '/') {
    // Absolute path
    strncpy(output, path, outputSize - 1);
    output[outputSize - 1] = '\0';
  } else if (strcmp(path, "..") == 0) {
    // Go up one directory
    strncpy(output, currentDir, outputSize - 1);
    output[outputSize - 1] = '\0';
    
    // Remove trailing slash
    size_t len = strlen(output);
    if (len > 1 && output[len - 1] == '/') {
      output[len - 1] = '\0';
      len--;
    }
    
    // Find last slash
    char* lastSlash = strrchr(output, '/');
    if (lastSlash && lastSlash != output) {
      *(lastSlash + 1) = '\0';
    } else {
      strcpy(output, "/");
    }
  } else {
    // Relative path
    strncpy(output, currentDir, outputSize - 1);
    output[outputSize - 1] = '\0';
    
    size_t len = strlen(output);
    if (len > 0 && output[len - 1] != '/') {
      strncat(output, "/", outputSize - len - 1);
    }
    strncat(output, path, outputSize - strlen(output) - 1);
  }
}

void processCommand(const char* cmdLine, char* currentDir) {
  // Skip leading whitespace
  while (*cmdLine == ' ') cmdLine++;
  if (*cmdLine == '\0') return;
  
  // Parse command and arguments
  char cmd[64] = {0};
  char args[192] = {0};
  
  const char* space = strchr(cmdLine, ' ');
  if (space) {
    size_t cmdLen = space - cmdLine;
    if (cmdLen >= sizeof(cmd)) cmdLen = sizeof(cmd) - 1;
    strncpy(cmd, cmdLine, cmdLen);
    cmd[cmdLen] = '\0';
    
    // Copy args, skipping whitespace
    space++;
    while (*space == ' ') space++;
    strncpy(args, space, sizeof(args) - 1);
    args[sizeof(args) - 1] = '\0';
  } else {
    strncpy(cmd, cmdLine, sizeof(cmd) - 1);
    cmd[sizeof(cmd) - 1] = '\0';
  }
  
  // Execute commands
  if (strcmp(cmd, "help") == 0) {
    cmdHelp();
  } else if (strcmp(cmd, "ls") == 0) {
    cmdLs(args, currentDir);
  } else if (strcmp(cmd, "cd") == 0) {
    cmdCd(args, currentDir);
  } else if (strcmp(cmd, "pwd") == 0) {
    cmdPwd(currentDir);
  } else if (strcmp(cmd, "cat") == 0) {
    cmdCat(args, currentDir);
  } else if (strcmp(cmd, "rm") == 0) {
    cmdRm(args, currentDir);
  } else if (strcmp(cmd, "touch") == 0) {
    cmdTouch(args, currentDir);
  } else if (strcmp(cmd, "mkdir") == 0) {
    cmdMkdir(args, currentDir);
  } else if (strcmp(cmd, "rmdir") == 0) {
    cmdRmdir(args, currentDir);
  } else if (strcmp(cmd, "echo") == 0) {
    cmdEcho(cmdLine, currentDir);
  } else if (strcmp(cmd, "grep") == 0) {
    cmdGrep(cmdLine, currentDir);
  } else if (strcmp(cmd, "mv") == 0) {
    cmdMv(args, currentDir);
  } else if (strcmp(cmd, "cp") == 0) {
    cmdCp(args, currentDir);
  } else if (strcmp(cmd, "ps") == 0) {
    Kernel::printTaskList();
  } else if (strcmp(cmd, "meminfo") == 0) {
    Kernel::printMemoryInfo();
  } else if (strcmp(cmd, "compact") == 0) {
    cmdCompact();
  } else if (strcmp(cmd, "uptime") == 0) {
    cmdUptime();
  } else if (strcmp(cmd, "clear") == 0) {
  cmdClear();
  } else if (strcmp(cmd, "edit") == 0) {
    cmdEdit(args, currentDir);
  }  else if (strcmp(cmd, "hwinfo") == 0) {
    cmdHwinfo();
  } else {
    Serial.print(F("Unknown command: "));
    Serial.println(cmd);
    Serial.println(F("Type 'help' for available commands"));
  }
}

void cmdHelp() {
  Serial.println(F("\nFile Operations:"));
  Serial.println(F("  ls [path]           - List files"));
  Serial.println(F("  cd <path>           - Change directory"));
  Serial.println(F("  pwd                 - Print working directory"));
  Serial.println(F("  cat <file>          - Display file"));
  Serial.println(F("  edit <file>         - Edit text file"));
  Serial.println(F("  grep <pattern> <file> - Search in file"));
  Serial.println(F("  rm <file>           - Remove file"));
  Serial.println(F("  mv <src> <dst>      - Move/rename file"));
  Serial.println(F("  cp <src> <dst>      - Copy file"));
  Serial.println(F("  touch <file>        - Create file"));
  Serial.println(F("  mkdir <dir>         - Create directory"));
  Serial.println(F("  rmdir <dir>         - Remove directory"));
  Serial.println(F("  echo [text]         - Print text"));
  Serial.println(F("  echo <text> > <file>- Write to file"));
  
  Serial.println(F("\nSystem Operations:"));
  Serial.println(F("  ps                  - List tasks"));
  Serial.println(F("  meminfo             - Memory info"));
  Serial.println(F("  hwinfo              - Hardware Info"));
  Serial.println(F("  compact             - Compact memory"));
  Serial.println(F("  uptime              - System uptime"));
  Serial.println(F("  clear               - Clear screen"));
  Serial.println(F("  help                - Show this help\n"));
}

void cmdLs(const char* path, const char* currentDir) {
  char fullPath[128];
  
  if (path[0] == '\0') {
    strncpy(fullPath, currentDir, sizeof(fullPath) - 1);
  } else {
    resolvePath(path, currentDir, fullPath, sizeof(fullPath));
  }
  fullPath[sizeof(fullPath) - 1] = '\0';
  
  int dh = OS::opendir(fullPath);
  if (dh < 0) {
    Serial.println(F("Error: Cannot open directory"));
    return;
  }
  
  Serial.println();
  
  DirEntry entry;
  while (OS::readdir(dh, &entry)) {
    if (entry.isDirectory) {
      Serial.print(F("  [DIR]  "));
      Serial.println(entry.name);
    } else {
      Serial.print(F("  [FILE] "));
      Serial.print(entry.name);
      Serial.print(F("\t\t"));
      Serial.print(entry.size);
      Serial.println(F(" bytes"));
    }
  }
  
  Serial.println();
  OS::closedir(dh);
}

void cmdCd(const char* path, char* currentDir) {
  if (path[0] == '\0') {
    strcpy(currentDir, "/");
    return;
  }
  
  char newPath[128];
  resolvePath(path, currentDir, newPath, sizeof(newPath));
  
  int dh = OS::opendir(newPath);
  if (dh < 0) {
    Serial.println(F("Error: Directory not found"));
    return;
  }
  
  OS::closedir(dh);
  strcpy(currentDir, newPath);
  
  size_t len = strlen(currentDir);
  if (len > 0 && currentDir[len - 1] != '/') {
    strncat(currentDir, "/", 127 - len);
  }
}

void cmdPwd(const char* currentDir) {
  Serial.println(currentDir);
}

void cmdCat(const char* filename, const char* currentDir) {
  if (filename[0] == '\0') {
    Serial.println(F("Error: No filename"));
    return;
  }
  
  char filepath[128];
  resolvePath(filename, currentDir, filepath, sizeof(filepath));
  
  int fd = OS::open(filepath, false);
  if (fd < 0) {
    Serial.println(F("Error: Cannot open file"));
    return;
  }
  
  Serial.println();
  char buffer[128];
  int bytesRead;
  
  while ((bytesRead = OS::read(fd, buffer, sizeof(buffer) - 1)) > 0) {
    buffer[bytesRead] = '\0';
    Serial.print(buffer);
  }
  
  Serial.println();
  OS::close(fd);
}

void cmdRm(const char* filename, const char* currentDir) {
  if (filename[0] == '\0') {
    Serial.println(F("Error: No filename"));
    return;
  }
  
  char filepath[128];
  resolvePath(filename, currentDir, filepath, sizeof(filepath));
  
  if (OS::remove(filepath)) {
    Serial.println(F("File removed"));
  } else {
    Serial.println(F("Error: Cannot remove file"));
  }
}

void cmdTouch(const char* filename, const char* currentDir) {
  if (filename[0] == '\0') {
    Serial.println(F("Error: No filename"));
    return;
  }
  
  char filepath[128];
  resolvePath(filename, currentDir, filepath, sizeof(filepath));
  
  int fd = OS::open(filepath, true);
  if (fd >= 0) {
    OS::close(fd);
    Serial.println(F("File created"));
  } else {
    Serial.println(F("Error: Cannot create file"));
  }
}

void cmdMkdir(const char* dirname, const char* currentDir) {
  if (dirname[0] == '\0') {
    Serial.println(F("Error: No directory name"));
    return;
  }
  
  char dirpath[128];
  resolvePath(dirname, currentDir, dirpath, sizeof(dirpath));
  
  if (OS::mkdir(dirpath)) {
    Serial.println(F("Directory created"));
  } else {
    Serial.println(F("Error: Cannot create directory"));
  }
}

void cmdRmdir(const char* dirname, const char* currentDir) {
  if (dirname[0] == '\0') {
    Serial.println(F("Error: No directory name"));
    return;
  }
  
  char dirpath[128];
  resolvePath(dirname, currentDir, dirpath, sizeof(dirpath));
  
  if (OS::rmdir(dirpath)) {
    Serial.println(F("Directory removed"));
  } else {
    Serial.println(F("Error: Cannot remove directory (must be empty)"));
  }
}

void cmdEcho(const char* fullCmd, const char* currentDir) {
  const char* redirectPos = strstr(fullCmd, ">");
  
  if (redirectPos) {
    // Echo to file
    char text[128] = {0};
    const char* textStart = fullCmd + 5; // Skip "echo "
    while (*textStart == ' ') textStart++;
    
    size_t textLen = redirectPos - textStart;
    while (textLen > 0 && textStart[textLen - 1] == ' ') textLen--;
    
    if (textLen >= sizeof(text)) textLen = sizeof(text) - 1;
    strncpy(text, textStart, textLen);
    text[textLen] = '\0';
    
    char filename[64] = {0};
    const char* filenameStart = redirectPos + 1;
    while (*filenameStart == ' ') filenameStart++;
    
    strncpy(filename, filenameStart, sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';
    
    size_t fnLen = strlen(filename);
    while (fnLen > 0 && filename[fnLen - 1] == ' ') {
      filename[--fnLen] = '\0';
    }
    
    if (filename[0] == '\0') {
      Serial.println(F("Error: No filename"));
      return;
    }
    
    char filepath[128];
    resolvePath(filename, currentDir, filepath, sizeof(filepath));
    
    OS::remove(filepath);
    
    int fd = OS::open(filepath, true);
    if (fd >= 0) {
      OS::write(fd, text, strlen(text));
      OS::write(fd, "\n", 1);
      OS::close(fd);
      Serial.println(F("Text written to file"));
    } else {
      Serial.println(F("Error: Cannot write to file"));
    }
  } else {
    // Echo to terminal
    const char* text = fullCmd + 5; // Skip "echo "
    while (*text == ' ') text++;
    Serial.println(text);
  }
}

void cmdCompact() {
  Serial.println(F("Compacting memory..."));
  OS::compact();
  Serial.println(F("Done"));
}

void cmdUptime() {
  uint32_t up = OS::uptime();
  uint32_t seconds = up / 1000;
  uint32_t minutes = seconds / 60;
  uint32_t hours = minutes / 60;
  
  Serial.print(F("Uptime: "));
  Serial.print(hours);
  Serial.print(F("h "));
  Serial.print(minutes % 60);
  Serial.print(F("m "));
  Serial.print(seconds % 60);
  Serial.println(F("s"));
}

void cmdClear() {
  for (int i = 0; i < 50; i++) {
    Serial.println();
  }
  Serial.println(F("================================="));
  Serial.println(F("  YandereOS Shell"));
  Serial.println(F("=================================\n"));
}

void cmdGrep(const char* fullCmd, const char* currentDir) {
  // Parse: grep <pattern> <file>
  const char* args = fullCmd + 5; // Skip "grep "
  while (*args == ' ') args++;
  
  char pattern[64] = {0};
  char filename[64] = {0};
  
  // Extract pattern (first argument)
  const char* space = strchr(args, ' ');
  if (!space) {
    Serial.println(F("Usage: grep <pattern> <file>"));
    return;
  }
  
  size_t patternLen = space - args;
  if (patternLen >= sizeof(pattern)) patternLen = sizeof(pattern) - 1;
  strncpy(pattern, args, patternLen);
  pattern[patternLen] = '\0';
  
  // Extract filename (second argument)
  const char* filenameStart = space + 1;
  while (*filenameStart == ' ') filenameStart++;
  strncpy(filename, filenameStart, sizeof(filename) - 1);
  filename[sizeof(filename) - 1] = '\0';
  
  // Trim trailing spaces
  size_t fnLen = strlen(filename);
  while (fnLen > 0 && filename[fnLen - 1] == ' ') {
    filename[--fnLen] = '\0';
  }
  
  if (filename[0] == '\0') {
    Serial.println(F("Usage: grep <pattern> <file>"));
    return;
  }
  
  // Open file
  char filepath[128];
  resolvePath(filename, currentDir, filepath, sizeof(filepath));
  
  int fd = OS::open(filepath, false);
  if (fd < 0) {
    Serial.println(F("Error: Cannot open file"));
    return;
  }
  
  // Read file line by line and search for pattern
  char line[128];
  int linePos = 0;
  bool foundMatch = false;
  
  Serial.println();
  
  char buffer[128];
  int bytesRead;
  while ((bytesRead = OS::read(fd, buffer, sizeof(buffer) - 1)) > 0) {
    for (int i = 0; i < bytesRead; i++) {
      char c = buffer[i];
      
      if (c == '\n' || c == '\r') {
        if (linePos > 0) {
          line[linePos] = '\0';
          
          // Simple substring search (case-sensitive)
          if (strstr(line, pattern) != nullptr) {
            Serial.println(line);
            foundMatch = true;
          }
          
          linePos = 0;
        }
      } else if (linePos < sizeof(line) - 1) {
        line[linePos++] = c;
      }
    }
  }
  
  // Check last line
  if (linePos > 0) {
    line[linePos] = '\0';
    if (strstr(line, pattern) != nullptr) {
      Serial.println(line);
      foundMatch = true;
    }
  }
  
  if (!foundMatch) {
    Serial.println(F("No matches found"));
  }
  
  Serial.println();
  OS::close(fd);
}

void cmdMv(const char* args, const char* currentDir) {
  // Parse: mv <src> <dst>
  char src[64] = {0};
  char dst[64] = {0};
  
  const char* space = strchr(args, ' ');
  if (!space) {
    Serial.println(F("Usage: mv <source> <destination>"));
    return;
  }
  
  // Extract source
  size_t srcLen = space - args;
  if (srcLen >= sizeof(src)) srcLen = sizeof(src) - 1;
  strncpy(src, args, srcLen);
  src[srcLen] = '\0';
  
  // Extract destination
  const char* dstStart = space + 1;
  while (*dstStart == ' ') dstStart++;
  strncpy(dst, dstStart, sizeof(dst) - 1);
  dst[sizeof(dst) - 1] = '\0';
  
  // Trim trailing spaces
  size_t dstLen = strlen(dst);
  while (dstLen > 0 && dst[dstLen - 1] == ' ') {
    dst[--dstLen] = '\0';
  }
  
  if (dst[0] == '\0') {
    Serial.println(F("Usage: mv <source> <destination>"));
    return;
  }
  
  // Resolve paths
  char srcPath[128];
  char dstPath[128];
  resolvePath(src, currentDir, srcPath, sizeof(srcPath));
  resolvePath(dst, currentDir, dstPath, sizeof(dstPath));
  
  // Check if source exists
  if (!OS::exists(srcPath)) {
    Serial.println(F("Error: Source file not found"));
    return;
  }
  
  // Copy file contents
  int fdSrc = OS::open(srcPath, false);
  if (fdSrc < 0) {
    Serial.println(F("Error: Cannot open source file"));
    return;
  }
  
  // Remove destination if it exists
  OS::remove(dstPath);
  
  int fdDst = OS::open(dstPath, true);
  if (fdDst < 0) {
    OS::close(fdSrc);
    Serial.println(F("Error: Cannot create destination file"));
    return;
  }
  
  // Copy data
  char buffer[128];
  int bytesRead;
  while ((bytesRead = OS::read(fdSrc, buffer, sizeof(buffer))) > 0) {
    OS::write(fdDst, buffer, bytesRead);
    OS::yield(); // Yield during long operation
  }
  
  OS::close(fdSrc);
  OS::close(fdDst);
  
  // Remove source file
  if (OS::remove(srcPath)) {
    Serial.println(F("File moved"));
  } else {
    Serial.println(F("Warning: Copy succeeded but could not delete source"));
  }
}

void cmdCp(const char* args, const char* currentDir) {
  // Parse: cp <src> <dst>
  char src[64] = {0};
  char dst[64] = {0};
  
  const char* space = strchr(args, ' ');
  if (!space) {
    Serial.println(F("Usage: cp <source> <destination>"));
    return;
  }
  
  // Extract source
  size_t srcLen = space - args;
  if (srcLen >= sizeof(src)) srcLen = sizeof(src) - 1;
  strncpy(src, args, srcLen);
  src[srcLen] = '\0';
  
  // Extract destination
  const char* dstStart = space + 1;
  while (*dstStart == ' ') dstStart++;
  strncpy(dst, dstStart, sizeof(dst) - 1);
  dst[sizeof(dst) - 1] = '\0';
  
  // Trim trailing spaces
  size_t dstLen = strlen(dst);
  while (dstLen > 0 && dst[dstLen - 1] == ' ') {
    dst[--dstLen] = '\0';
  }
  
  if (dst[0] == '\0') {
    Serial.println(F("Usage: cp <source> <destination>"));
    return;
  }
  
  // Resolve paths
  char srcPath[128];
  char dstPath[128];
  resolvePath(src, currentDir, srcPath, sizeof(srcPath));
  resolvePath(dst, currentDir, dstPath, sizeof(dstPath));
  
  // Check if source exists
  if (!OS::exists(srcPath)) {
    Serial.println(F("Error: Source file not found"));
    return;
  }
  
  // Open source
  int fdSrc = OS::open(srcPath, false);
  if (fdSrc < 0) {
    Serial.println(F("Error: Cannot open source file"));
    return;
  }
  
  // Remove destination if it exists
  OS::remove(dstPath);
  
  // Open destination
  int fdDst = OS::open(dstPath, true);
  if (fdDst < 0) {
    OS::close(fdSrc);
    Serial.println(F("Error: Cannot create destination file"));
    return;
  }
  
  // Copy data
  char buffer[128];
  int bytesRead;
  while ((bytesRead = OS::read(fdSrc, buffer, sizeof(buffer))) > 0) {
    OS::write(fdDst, buffer, bytesRead);
    OS::yield(); // Yield during long operation
  }
  
  OS::close(fdSrc);
  OS::close(fdDst);
  
  Serial.println(F("File copied"));
}

/*
  Ed-style Text Editor
  Add this function with the other cmd* functions in your shell
*/

/*
  Ed-style Text Editor
  Add this function with the other cmd* functions in your shell
*/

void cmdEdit(const char* filename, const char* currentDir) {
  if (filename[0] == '\0') {
    Serial.println(F("Usage: edit <filename>"));
    return;
  }
  
  // Allocate editor buffer on heap instead of stack
  #define MAX_LINES 200
  char (*lines)[128] = (char (*)[128])OS::malloc(MAX_LINES * 128);
  if (!lines) {
    Serial.println(F("Error: Out of memory"));
    return;
  }
  
  int lineCount = 0;
  bool modified = false;
  
  char filepath[128];
  resolvePath(filename, currentDir, filepath, sizeof(filepath));
  
  // Load file
  int fd = OS::open(filepath, false);
  if (fd >= 0) {
    char currentLine[128];
    int linePos = 0;
    char buffer[128];
    int bytesRead;
    
    while ((bytesRead = OS::read(fd, buffer, sizeof(buffer) - 1)) > 0) {
      for (int i = 0; i < bytesRead; i++) {
        char c = buffer[i];
        if (c == '\n' || c == '\r') {
          if (linePos > 0 || c == '\n') {
            if (lineCount >= MAX_LINES) {
              Serial.println(F("Warning: File too large, truncated"));
              break;
            }
            currentLine[linePos] = '\0';
            strcpy(lines[lineCount++], currentLine);
            linePos = 0;
          }
        } else if (linePos < 127) {
          currentLine[linePos++] = c;
        }
      }
      OS::yield();
    }
    
    if (linePos > 0 && lineCount < MAX_LINES) {
      currentLine[linePos] = '\0';
      strcpy(lines[lineCount++], currentLine);
    }
    
    OS::close(fd);
    Serial.print(lineCount);
    Serial.println(F(" lines loaded"));
  } else {
    Serial.println(F("New file"));
  }
  
  Serial.println(F("\nEd-style line editor. Type 'h' for help.\n"));
  
  // Command loop
  char cmd[128];
  while (true) {
    Serial.print(F(": "));
    
    // Read command
    int pos = 0;
    cmd[0] = '\0';
    while (true) {
      while (Serial.available() > 0) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
          if (pos > 0) {
            Serial.println();
            cmd[pos] = '\0';
            goto process_command;
          }
        } else if (c == 127 || c == 8) {
          if (pos > 0) {
            pos--;
            Serial.write(8);
            Serial.write(' ');
            Serial.write(8);
          }
        } else if (pos < 127) {
          cmd[pos++] = c;
          Serial.write(c);
        }
      }
      OS::yield();
    }
    
process_command:
    if (strlen(cmd) == 0) continue;
    
    char cmdChar = cmd[0];
    
    // Help
    if (strcmp(cmd, "h") == 0) {
      Serial.println(F("\nEditor Commands:"));
      Serial.println(F("  p           - Print all lines"));
      Serial.println(F("  p N         - Print line N"));
      Serial.println(F("  p N,M       - Print lines N to M"));
      Serial.println(F("  a           - Append lines (end with '.')"));
      Serial.println(F("  i N         - Insert before line N (end with '.')"));
      Serial.println(F("  d N         - Delete line N"));
      Serial.println(F("  d N,M       - Delete lines N to M"));
      Serial.println(F("  c N <text>  - Change line N to <text>"));
      Serial.println(F("  w           - Write/save file"));
      Serial.println(F("  q           - Quit (warns if unsaved)"));
      Serial.println(F("  q!          - Quit without saving"));
      Serial.println(F("  h           - Show this help\n"));
      
    // Print
    } else if (strcmp(cmd, "p") == 0) {
      Serial.println();
      for (int i = 0; i < lineCount; i++) {
        Serial.print(i + 1);
        Serial.print(F(": "));
        Serial.println(lines[i]);
      }
      Serial.println();
      
    } else if (cmdChar == 'p' && strlen(cmd) > 1) {
      char* args = cmd + 2;
      while (*args == ' ') args++;
      
      char* comma = strchr(args, ',');
      if (comma) {
        *comma = '\0';
        int start = atoi(args);
        int end = atoi(comma + 1);
        if (start < 1) start = 1;
        if (end > lineCount) end = lineCount;
        Serial.println();
        for (int i = start - 1; i < end; i++) {
          Serial.print(i + 1);
          Serial.print(F(": "));
          Serial.println(lines[i]);
        }
        Serial.println();
      } else {
        int lineNum = atoi(args);
        if (lineNum >= 1 && lineNum <= lineCount) {
          Serial.println();
          Serial.print(lineNum);
          Serial.print(F(": "));
          Serial.println(lines[lineNum - 1]);
          Serial.println();
        }
      }
      
    // Append
    } else if (strcmp(cmd, "a") == 0) {
      Serial.println(F("Append mode (type '.' alone to end):"));
      char line[128];
      while (true) {
        int lpos = 0;
        line[0] = '\0';
        while (true) {
          while (Serial.available() > 0) {
            char c = Serial.read();
            if (c == '\n' || c == '\r') {
              if (lpos > 0) {
                Serial.println();
                line[lpos] = '\0';
                goto append_line;
              }
            } else if (c == 127 || c == 8) {
              if (lpos > 0) {
                lpos--;
                Serial.write(8);
                Serial.write(' ');
                Serial.write(8);
              }
            } else if (lpos < 127) {
              line[lpos++] = c;
              Serial.write(c);
            }
          }
          OS::yield();
        }
append_line:
        if (strcmp(line, ".") == 0) break;
        if (lineCount >= MAX_LINES) {
          Serial.println(F("Error: Editor buffer full"));
          break;
        }
        strcpy(lines[lineCount++], line);
        modified = true;
      }
      
    // Insert
    } else if (cmdChar == 'i' && strlen(cmd) > 1) {
      int beforeLine = atoi(cmd + 2);
      if (beforeLine < 1 || beforeLine > lineCount + 1) {
        Serial.println(F("Error: Invalid line number"));
      } else {
        Serial.println(F("Insert mode (type '.' alone to end):"));
        
        // Allocate temp buffer for new lines
        char (*newLines)[128] = (char (*)[128])OS::malloc(MAX_LINES * 128);
        if (!newLines) {
          Serial.println(F("Error: Out of memory"));
        } else {
          int newCount = 0;
          
          char line[128];
          while (true) {
            int lpos = 0;
            line[0] = '\0';
            while (true) {
              while (Serial.available() > 0) {
                char c = Serial.read();
                if (c == '\n' || c == '\r') {
                  if (lpos > 0) {
                    Serial.println();
                    line[lpos] = '\0';
                    goto insert_line;
                  }
                } else if (c == 127 || c == 8) {
                  if (lpos > 0) {
                    lpos--;
                    Serial.write(8);
                    Serial.write(' ');
                    Serial.write(8);
                  }
                } else if (lpos < 127) {
                  line[lpos++] = c;
                  Serial.write(c);
                }
              }
              OS::yield();
            }
insert_line:
            if (strcmp(line, ".") == 0) break;
            if (newCount >= MAX_LINES) {
              Serial.println(F("Error: Too many lines"));
              break;
            }
            strcpy(newLines[newCount++], line);
          }
          
          if (newCount > 0 && lineCount + newCount <= MAX_LINES) {
            for (int i = lineCount - 1; i >= beforeLine - 1; i--) {
              strcpy(lines[i + newCount], lines[i]);
            }
            for (int i = 0; i < newCount; i++) {
              strcpy(lines[beforeLine - 1 + i], newLines[i]);
            }
            lineCount += newCount;
            modified = true;
          }
          
          OS::free(newLines);
        }
      }
      
    // Delete
    } else if (cmdChar == 'd' && strlen(cmd) > 1) {
      char* args = cmd + 2;
      while (*args == ' ') args++;
      
      char* comma = strchr(args, ',');
      int start, end;
      if (comma) {
        *comma = '\0';
        start = atoi(args);
        end = atoi(comma + 1);
      } else {
        start = end = atoi(args);
      }
      
      if (start < 1 || start > lineCount) {
        Serial.println(F("Error: Invalid line number"));
      } else {
        if (end < start) end = start;
        if (end > lineCount) end = lineCount;
        
        int deleteCount = end - start + 1;
        for (int i = end; i < lineCount; i++) {
          strcpy(lines[i - deleteCount], lines[i]);
        }
        lineCount -= deleteCount;
        modified = true;
        
        Serial.print(deleteCount);
        Serial.println(F(" line(s) deleted"));
      }
      
    // Change
    } else if (cmdChar == 'c' && strlen(cmd) > 1) {
      char* args = cmd + 2;
      while (*args == ' ') args++;
      
      char* space = strchr(args, ' ');
      if (space) {
        *space = '\0';
        int lineNum = atoi(args);
        if (lineNum >= 1 && lineNum <= lineCount) {
          strncpy(lines[lineNum - 1], space + 1, 127);
          lines[lineNum - 1][127] = '\0';
          modified = true;
          Serial.println(F("Line changed"));
        } else {
          Serial.println(F("Error: Invalid line number"));
        }
      } else {
        Serial.println(F("Usage: c <line> <text>"));
      }
      
    // Write
    } else if (strcmp(cmd, "w") == 0) {
      OS::remove(filepath);
      fd = OS::open(filepath, true);
      if (fd >= 0) {
        for (int i = 0; i < lineCount; i++) {
          OS::write(fd, lines[i], strlen(lines[i]));
          OS::write(fd, "\n", 1);
          OS::yield();
        }
        OS::close(fd);
        modified = false;
        Serial.print(lineCount);
        Serial.println(F(" lines written"));
      } else {
        Serial.println(F("Error: Cannot write file"));
      }
      
    // Quit
    } else if (strcmp(cmd, "q") == 0) {
      if (modified) {
        Serial.println(F("Warning: Unsaved changes! Use 'q!' to quit without saving"));
      } else {
        break;
      }
      
    } else if (strcmp(cmd, "q!") == 0) {
      break;
      
    } else {
      Serial.println(F("Unknown command. Type 'h' for help"));
    }
    
    OS::yield();
  }
  
  OS::free(lines);
  Serial.println(F("Editor closed"));
}

void cmdHwinfo() {
  Serial.println(F("\n=== Hardware Info ==="));
  
  #ifdef ARDUINO_GIGA
    Serial.println(F("Board: Arduino Giga R1 WiFi"));
    Serial.println(F("MCU: STM32H747XI (Cortex-M7 + M4)"));
    Serial.println(F("RAM: 1 MB"));
    Serial.println(F("Flash: 2 MB"));
  #elif defined(ARDUINO_ARCH_RP2040)
    Serial.println(F("Board: Raspberry Pi Pico"));
    Serial.println(F("MCU: RP2040 (Dual Cortex-M0+)"));
    Serial.println(F("RAM: 264 KB"));
    Serial.println(F("Flash: 2 MB"));
  #elif defined(ARDUINO_AVR_MEGA2560)
    Serial.println(F("Board: Arduino Mega 2560"));
    Serial.println(F("MCU: ATmega2560"));
    Serial.println(F("RAM: 8 KB"));
    Serial.println(F("Flash: 256 KB"));
  #else
    Serial.println(F("Board: Unknown/Generic Arduino"));
  #endif
  
  Serial.print(F("Clock: "));
  Serial.print(F_CPU / 1000000);
  Serial.println(F(" MHz"));
  
  Serial.print(F("Kernel heap: "));
  Serial.print(KERNEL_HEAP_SIZE);
  Serial.println(F(" bytes"));
  
  Serial.print(F("SD CS Pin: "));
  Serial.println(SD_CS_PIN);
  
  Serial.println();
}

// ============================================================================
// MAIN PROGRAM
// ============================================================================

void setup() {
  // Initialize kernel
  if (!Kernel::init()) {
    Serial.println(F("FATAL: Kernel init failed"));
    while(1);
  }
  
  // Create shell task
  int shellTaskId = Kernel::createTask("shell", shellTask);
  if (shellTaskId < 0) {
    Kernel::panic("Failed to create shell task");
  }
  
  Serial.println(F("Shell ready. Type 'help' for commands.\n"));
}

void loop() {
  Kernel::schedule();
  delay(1);
}

// ============================================================================
// EXAMPLE USAGE
// ============================================================================
/*
Try these commands:

  mkdir /mydir
  cd /mydir
  touch hello.txt
  echo Hello World > hello.txt
  cat hello.txt
  ls
  cd ..
  ls
  rm /mydir/hello.txt
  rmdir /mydir
  
  ps         - See all tasks
  meminfo    - Check memory usage
  compact    - Compact memory if fragmented
  uptime     - System uptime
*/
