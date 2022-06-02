#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

unsigned char nullPtr[16] = { 0 };

unsigned char initalPointers[24] = { 0 };

int getSize(int inSize) {
  if (inSize < 16) return 16;
  return inSize;
}

uint64_t toUINT64(unsigned char *buf) {
    uint64_t result = buf[7];
    result |= ((uint64_t) buf[6] << 8);
    result |= ((uint64_t) buf[5] << 16);
    result |= ((uint64_t) buf[4] << 24);
    result |= ((uint64_t) buf[3] << 32);
    result |= ((uint64_t) buf[2] << 40);
    result |= ((uint64_t) buf[1] << 48);
    result |= ((uint64_t) buf[0] << 56);
    return result;
}

void _fromUINT64(unsigned char *result, uint64_t uint) {
  result[7] = uint & 0xff;
  result[6] = (uint >> 8) & 0xff;
  result[5] = (uint >> 16) & 0xff;
  result[4] = (uint >> 24) & 0xff;
  result[3] = (uint >> 32) & 0xff;
  result[2] = (uint >> 40) & 0xff;
  result[1] = (uint >> 48) & 0xff;
  result[0] = (uint >> 56) & 0xff;
}

unsigned char *fromUINT64(uint64_t uint) {
  unsigned char *result = (unsigned char *)malloc(8);
  _fromUINT64(result, uint);
  return result;
}

unsigned short int toUINT16(unsigned char *buf) {
    unsigned short int result = buf[1];
    result |= ((unsigned short int) buf[0] << 8);
    return result;
}

unsigned char *encodeBlockInfo(int size) {
  unsigned char *buf = (unsigned char *)malloc(2);
  buf[0] = (size >> 7);
  buf[1] = (((size & 0xff) << 1) | 1);

  return buf;
}

uint64_t _allocEnd(FILE *fp, int size) {
  fseek (fp, 0, SEEK_END);
  uint64_t pos = ftello64(fp);

  unsigned char *blockInfo = encodeBlockInfo(size);
  fwrite (blockInfo, 1, 2, fp);
  fseek (fp, size, SEEK_CUR);
  fwrite (blockInfo, 1, 2, fp);

  return pos + 2ULL;
}

void log(unsigned char *arr, int len) {
  for (int i = 0; i < len; i++) {
    printf("%02x",arr[i]);
  }
  printf("\n");
}

void _setFreeInfo(FILE *fp, uint64_t next, uint64_t prev) {
  unsigned char freeInfo[16] = { 0 };
  _fromUINT64(freeInfo, next);
  _fromUINT64(freeInfo + 8, prev);

  fwrite (freeInfo, 1, 16, fp);
}

void _unlinkFreeBlock(FILE *fp, unsigned char *freeInfo) {
  uint64_t nextPtr = toUINT64(freeInfo);
  uint64_t prevPtr = toUINT64(freeInfo + 8);

  unsigned char fileHeadOffset = 0;
  unsigned char fileHeadWrite = 0;

  // Set NextPtr's Previous to PrevPtr, if 0 then set end to PrevPtr

  if (nextPtr == 0) {
    fileHeadOffset = 8;
    fileHeadWrite = 8;
  } else {
    fseeko64(fp, nextPtr + 8, SEEK_SET);
    fwrite (freeInfo + 8, 1, 8, fp);
  }

  // Set PrevPtr's Next to NextPtr, if 0 then set head to NextPtr

  if (prevPtr == 0) {
    fileHeadOffset = 0;
    fileHeadWrite += 8;
  } else {
    fseeko64(fp, prevPtr, SEEK_SET);
    fwrite (freeInfo, 1, 8, fp);
  }

  if (fileHeadWrite != 0) {
    fseeko64(fp, 8 + fileHeadOffset, SEEK_SET);
    fwrite (freeInfo + fileHeadOffset, 1, fileHeadWrite, fp);
  }
}


uint64_t alloc(const char *filename, int inSize) {
  if (inSize > 0x7fff) return 0;
  int size = getSize(inSize);
  uint64_t returnPtr = 0;
  
  FILE *fp;

  fp = fopen (filename, "r+b");

  unsigned char freeHeader[16] = { 0 };

  fseek (fp, 8, SEEK_SET);
  fread (freeHeader, 1, 16, fp);


  if (memcmp(freeHeader, nullPtr, 8) == 0) {
    returnPtr = _allocEnd(fp, size);
  } else {
    printf("Free Block found.\n");
    uint64_t headPtr = toUINT64(freeHeader);
    uint64_t endPtr = toUINT64(freeHeader + 8);
    bool shallLoop = true;

    if (endPtr == 0) {
      printf("Fatal Error: End Pointer is NULL.\n");
      goto end;
    }

    unsigned char freeInfo[2] = { 0 };
    fseeko64 (fp, endPtr - 2, SEEK_SET);
    fread    (freeInfo, 1, 2, fp);
        
    unsigned short int headerSize = (toUINT16(freeInfo) & 0xfffe) >> 1;

    if (size > headerSize) {
      shallLoop = false;

      returnPtr = _allocEnd(fp, size);
    }

    if (shallLoop) {
      uint64_t currentPtr = headPtr;

        unsigned char currentResult[18] = { 0 };
        while (currentPtr != 0) {
          fseeko64 (fp, currentPtr - 2, SEEK_SET);
          int bytesRead = fread (currentResult, 1, 18, fp);

          if (bytesRead != 18) {
            printf("Fatal Error: Failed to read Block Info.\n");
            break;
          }

          unsigned short int freeSize = (toUINT16(currentResult) & 0xfffe) >> 1;

          if (freeSize >= size) {
            _unlinkFreeBlock(fp, currentResult + 2);

            currentResult[1] |= 1;
            fseeko64 (fp, currentPtr - 1, SEEK_SET);
            fwrite (currentResult + 1, 1, 1, fp);
            fseek  (fp, freeSize + 1, SEEK_CUR);
            fwrite (currentResult + 1, 1, 1, fp);
            returnPtr = currentPtr;
            break;
          } else {
            currentPtr = toUINT64(currentResult + 2);
          }
        }
    }
  }

end:
  fclose (fp);

  return returnPtr;
}

void free(const char *filename, uint64_t ptr) {
  if (ptr < 24) {
    printf("Illegal Pointer/Position\n");
    return;
  }
  FILE *fp;

  fp = fopen (filename, "r+b");

  unsigned char pointerInfo[2] = { 0 };

  fseeko64 (fp, ptr - 2, SEEK_SET);
  fread    (pointerInfo, 1, 2, fp);

  unsigned short int headerInfo = toUINT16(pointerInfo);
  unsigned short int headerSize = (headerInfo & 0xfffe) >> 1;
  bool isAllocated = (headerInfo & 1) == 1;
  
  if (isAllocated == false) {
    printf("This Pointer isnt even allocated ;-;\n");
  } else {
    pointerInfo[1] &= 0xfe;
    fseek  (fp, -1, SEEK_CUR);
    fwrite (pointerInfo + 1, 1, 1, fp);
    fseek  (fp, headerSize + 1, SEEK_CUR);
    fwrite (pointerInfo + 1, 1, 1, fp);

    unsigned char freeHeader[16] = { 0 };

    fseek (fp, 8, SEEK_SET);
    fread (freeHeader, 1, 16, fp);

    unsigned char ptrBuf[8] = { 0 };
    _fromUINT64 (ptrBuf, ptr);
  
    if (memcmp(freeHeader, nullPtr, 8) == 0) {

      // Set Head Free Pointer to freed pointer.

      fseek  (fp, 8, SEEK_SET);
      fwrite (ptrBuf, 1, 8, fp);
      fwrite (ptrBuf, 1, 8, fp);

      // Set Next and Prev to NULL.

      fseeko64 (fp, ptr, SEEK_SET);
      fwrite   (nullPtr, 1, 16, fp);

    } else {
      uint64_t headPtr = toUINT64(freeHeader);
      uint64_t endPtr = toUINT64(freeHeader + 8);

      bool shallLoop = true;
      bool shallEnd = true;

      if (headPtr != 0) {
        unsigned char freeInfo[18] = { 0 };
        fseeko64 (fp, headPtr - 2, SEEK_SET);
        fread    (freeInfo, 1, 18, fp);

        unsigned short int freeSize = (toUINT16(freeInfo) & 0xfffe) >> 1;

        if (headerSize <= freeSize) {
          shallEnd = false;
          shallLoop = false;

          // Set Previous for headPtr to ptrBuf.

          fseeko64 (fp, headPtr + 8, SEEK_SET);
          fwrite (ptrBuf, 1, 8, fp);

          // Set Next for ptr to Head Pointer (freeHeader)

          fseeko64 (fp, ptr, SEEK_SET);
          _setFreeInfo(fp, headPtr, 0);

          // Set Head Pointer (fseek(fp, 0, SEEK_SET)) to ptrBuf

          fseek (fp, 8, SEEK_SET);
          fwrite (ptrBuf, 1, 8, fp);
        }
      }

      if (endPtr == 0) {
        printf("Fatal Error: End Pointer is NULL.\n");
        goto end;
      }

      if (shallEnd) {
        unsigned char freeInfo[18] = { 0 };
        fseeko64 (fp, endPtr - 2, SEEK_SET);
        fread    (freeInfo, 1, 18, fp);
        
        unsigned short int freeSize = (toUINT16(freeInfo) & 0xfffe) >> 1;

        if (headerSize >= freeSize) {
          shallLoop = false;

          // Set Next for endPtr to ptrBuf.

          fseeko64 (fp, endPtr, SEEK_SET);
          fwrite (ptrBuf, 1, 8, fp);

          // Set Previous for ptr to End Pointer (freeHeader + 8)

          fseeko64 (fp, ptr, SEEK_SET);
          _setFreeInfo(fp, 0, endPtr);

          // Set End Pointer (fseek(fp, 8, SEEK_SET)) to ptrBuf

          fseek (fp, 16, SEEK_SET);
          fwrite (ptrBuf, 1, 8, fp);
        }
      }

      if (shallLoop) {
        uint64_t currentPtr = headPtr;

        unsigned char currentResult[18] = { 0 };
        while (currentPtr != 0) {
          fseeko64 (fp, currentPtr - 2, SEEK_SET);
          int bytesRead = fread (currentResult, 1, 18, fp);

          if (bytesRead != 18) {
            printf("Fatal Error: Failed to read Block Info.\n");
            break;
          }

          unsigned short int freeSize = (toUINT16(currentResult) & 0xfffe) >> 1;

          if (freeSize >= headerSize) {
            uint64_t prevPtr = toUINT64(currentResult + 10);

            // Set currentPtr's Previous to ptrBuf

            fseek (fp, -8, SEEK_CUR);
            fwrite (ptrBuf, 1, 8, fp);

            // Set prevPtr's Next to ptrBuf

            if (prevPtr != 0) {
              fseeko64 (fp, prevPtr, SEEK_SET);
              fwrite (ptrBuf, 1, 8, fp);
            }

            fseeko64 (fp, ptr, SEEK_SET);
            _setFreeInfo(fp, currentPtr, prevPtr);

            break;
          } else {
            currentPtr = toUINT64(currentResult + 2);
          }
        }
      }
    }
  }

end:
  fclose (fp);
}

void dWrite(const char *filename, uint64_t ptr, unsigned char *wContent, int len) {
  FILE *fp;

  fp = fopen (filename, "r+b");

  unsigned char pointerInfo[2] = { 0 };

  fseeko64 (fp, ptr - 2, SEEK_SET);
  int res = fread (pointerInfo, 1, 2, fp);
  unsigned short int headerInfo = toUINT16(pointerInfo);
  unsigned short int headerSize = (headerInfo & 0xfffe) >> 1;
  bool isAllocated = (headerInfo & 1) == 1;

  if (isAllocated == false) {
    printf("Block isn't allocated.\n");
  } else {
    if (len > headerSize) {
      printf("Buffer overflow.\n");
    } else {
      fseeko64 (fp, 0, SEEK_CUR);
      fwrite (wContent, 1, len, fp);
    }
  }

  fclose (fp);
}

void init(const char *filename) {
  FILE *fp;

  fp = fopen (filename, "wb");
  if (fp == NULL) {
      return;
  }
  
  // Set Entry Pointer, Head Pointer and End Pointer to NULL
  fwrite (initalPointers, 1, 24, fp);
   
  fclose(fp);
}
