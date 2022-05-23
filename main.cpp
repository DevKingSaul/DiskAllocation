#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

unsigned char nullPtr[] = {0,0,0,0,0,0,0,0};

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

unsigned char *fromUINT64(uint64_t uint) {
  unsigned char * result = (unsigned char *)malloc(8);
  result[7] = uint & 0xff;
  result[6] = (uint >> 8) & 0xff;
  result[5] = (uint >> 16) & 0xff;
  result[4] = (uint >> 24) & 0xff;
  result[3] = (uint >> 32) & 0xff;
  result[2] = (uint >> 40) & 0xff;
  result[1] = (uint >> 48) & 0xff;
  result[0] = (uint >> 56) & 0xff;
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
  fseek(fp, 0, SEEK_END);
  uint64_t pos = ftello64(fp);

  unsigned char *blockInfo = encodeBlockInfo(size);
  fwrite (blockInfo, 1, 2, fp);
  fseek (fp, size, SEEK_CUR);
  fwrite (blockInfo, 1, 2, fp);

  return pos + 2ULL;
}

uint64_t alloc(const char *filename, int inSize) {
  if (inSize > 0x7fff) return 0;
  int size = getSize(inSize);
  uint64_t returnPtr = 0;
  
  FILE *fp;

  fp = fopen (filename, "r+b");

  unsigned char *firstFreeBuf = (unsigned char *)malloc(8);

  fread (firstFreeBuf, 1, 8, fp);


  if (memcmp(firstFreeBuf, nullPtr, 8) == 0) {
    returnPtr = _allocEnd(fp, size);
  } else {
    printf("Free Block found.\n");
  }

  free (firstFreeBuf);

  fclose (fp);

  return returnPtr;
}

void free(const char *filename, uint64_t ptr) {
  FILE *fp;

  fp = fopen (filename, "r+b");

  unsigned char *pointerInfo = (unsigned char *)malloc(2);

  fseeko64(fp, ptr - 2, SEEK_SET);
  fread(pointerInfo, 1, 2, fp);

  unsigned short int headerInfo = toUINT16(pointerInfo);
  unsigned short int headerSize = (headerInfo & 0xfffe) >> 1;
  bool isAllocated = (headerInfo & 1) == 1;
  
  if (isAllocated == false) {
    printf("This Pointer isnt even allocated ;-;\n");
  } else {
    pointerInfo[1] &= 0xfe;
    fseek (fp, -1, SEEK_CUR);
    fwrite (pointerInfo + 1, 1, 1, fp);
    fseek (fp, headerSize + 1, SEEK_CUR);
    fwrite (pointerInfo + 1, 1, 1, fp);

    unsigned char *firstFreeBuf = (unsigned char *)malloc(8);

    fseek (fp, 0, SEEK_SET);
    fread (firstFreeBuf, 1, 8, fp);
  
    if (memcmp(firstFreeBuf, nullPtr, 8) == 0) {
      fseek (fp, -8, SEEK_CUR);
      unsigned char *ptrBuf = fromUINT64(ptr);
      fwrite (ptrBuf, 1, 8, fp);
      free(ptrBuf);
    } else {
      
    }

    free(firstFreeBuf);
  }

  free(pointerInfo);
  fclose (fp);
}

void init(const char *filename) {
  FILE *fp;

  fp = fopen (filename, "wb");
  if (fp == NULL) {
      return;
  }
  fwrite (nullPtr, 1, 8, fp);
   
  fclose(fp);
}

int main () {
  init("./mainDB");
  uint64_t ptr = alloc("./mainDB", 16);
  uint64_t ptr2 = alloc("./mainDB", 16);

  printf("Pointer #1: 0x%016llx\n", ptr);
  printf("Pointer #2: 0x%016llx\n", ptr2);

  free("./mainDB", ptr2);

  return 0;
}
