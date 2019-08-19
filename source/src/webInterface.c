#include <stdio.h>
#include <stdarg.h>
#include <root.h>
#include "mongoose.h"

#define WPRINTF		//printf

char *s_http_port = "8000";

#define LIVE_IMAGE_MAX_SIZE 1024*1024
#define MAX_HTML_PAGE_SIZE 1024*1024

char SourceInterface[MAX_HTML_PAGE_SIZE];
int	SourceInterfaceSize = 0;

char SourceSettings[MAX_HTML_PAGE_SIZE];
int	SourceSettingsSize = 0;

char liveImageBuffer[LIVE_IMAGE_MAX_SIZE];
int   liveImageBuffer_Size = 0;

extern int restartApplicationVar;

unsigned char TV[2033] =
{
    0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x60, 
    0x00, 0x60, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x03, 0x02, 0x02, 0x03, 0x02, 0x02, 0x03, 
    0x03, 0x03, 0x03, 0x04, 0x03, 0x03, 0x04, 0x05, 0x08, 0x05, 0x05, 0x04, 0x04, 0x05, 0x0A, 0x07, 
    0x07, 0x06, 0x08, 0x0C, 0x0A, 0x0C, 0x0C, 0x0B, 0x0A, 0x0B, 0x0B, 0x0D, 0x0E, 0x12, 0x10, 0x0D, 
    0x0E, 0x11, 0x0E, 0x0B, 0x0B, 0x10, 0x16, 0x10, 0x11, 0x13, 0x14, 0x15, 0x15, 0x15, 0x0C, 0x0F, 
    0x17, 0x18, 0x16, 0x14, 0x18, 0x12, 0x14, 0x15, 0x14, 0xFF, 0xDB, 0x00, 0x43, 0x01, 0x03, 0x04, 
    0x04, 0x05, 0x04, 0x05, 0x09, 0x05, 0x05, 0x09, 0x14, 0x0D, 0x0B, 0x0D, 0x14, 0x14, 0x14, 0x14, 
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 
    0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0x14, 0xFF, 0xC2, 
    0x00, 0x11, 0x08, 0x00, 0x48, 0x00, 0x80, 0x03, 0x01, 0x11, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 
    0x01, 0xFF, 0xC4, 0x00, 0x1A, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 0x00, 0x01, 0x09, 0x02, 0x08, 0xFF, 0xC4, 0x00, 
    0x1B, 0x01, 0x00, 0x02, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x05, 0x08, 0x01, 0x02, 0x00, 0x03, 0x04, 0x09, 0xFF, 0xDA, 0x00, 0x0C, 0x03, 0x01, 
    0x00, 0x02, 0x10, 0x03, 0x10, 0x00, 0x00, 0x01, 0xFD, 0x51, 0x5A, 0xCC, 0x7D, 0xCE, 0x2C, 0xE6, 
    0x66, 0x43, 0x8C, 0x23, 0xA2, 0xA3, 0xF3, 0x4D, 0x8D, 0xAB, 0xC6, 0x76, 0x60, 0x47, 0x7B, 0xD1, 
    0xAD, 0x32, 0x69, 0x5C, 0x39, 0xCD, 0x32, 0x15, 0xD2, 0x52, 0x22, 0x47, 0x84, 0xE2, 0xCE, 0x66, 
    0x63, 0xAF, 0xEA, 0x9D, 0x6B, 0x31, 0xF7, 0x38, 0xB3, 0x99, 0x99, 0x0E, 0x30, 0x8E, 0x8A, 0x8F, 
    0xCD, 0x36, 0x36, 0xAF, 0x19, 0xD9, 0x81, 0x1D, 0xEF, 0x46, 0xB4, 0xC9, 0xA5, 0x70, 0xE7, 0x34, 
    0xC8, 0x57, 0x49, 0x48, 0x89, 0x1E, 0x13, 0x8B, 0x39, 0x99, 0x8E, 0xBF, 0xAA, 0x75, 0xAC, 0xC7, 
    0xDC, 0xE2, 0xCE, 0x66, 0x64, 0x38, 0xC2, 0x3A, 0x2A, 0x3F, 0x34, 0xD8, 0xDA, 0xBC, 0x67, 0x66, 
    0x04, 0x77, 0xBD, 0x1A, 0xD3, 0x26, 0x95, 0xC3, 0x9C, 0xD3, 0x21, 0x5D, 0x25, 0x22, 0x24, 0x78, 
    0x4E, 0x2C, 0xE6, 0x66, 0x3A, 0xFE, 0xA9, 0xD6, 0xB3, 0x1F, 0x73, 0x8B, 0x39, 0x99, 0x90, 0xE3, 
    0x08, 0xE8, 0xA8, 0xFC, 0xD3, 0x63, 0x6A, 0xF1, 0x9D, 0x98, 0x11, 0xDE, 0xF4, 0x6B, 0x4C, 0x9A, 
    0x57, 0x0E, 0x73, 0x4C, 0x85, 0x74, 0x94, 0x98, 0x91, 0xE0, 0x38, 0xB3, 0x99, 0x98, 0xEB, 0xFA, 
    0xA7, 0x5A, 0xCC, 0x7D, 0xCE, 0x2C, 0xE6, 0x66, 0x43, 0x8C, 0x23, 0xA2, 0xA3, 0xF3, 0x4D, 0x8D, 
    0xAB, 0xC6, 0x76, 0x60, 0x47, 0x7B, 0xD1, 0xAD, 0x32, 0x69, 0x5C, 0x39, 0xCD, 0x32, 0x15, 0xD2, 
    0x52, 0x62, 0x47, 0x80, 0xE2, 0xCE, 0x66, 0x63, 0xAF, 0xEA, 0x9D, 0x6B, 0x31, 0xF7, 0x38, 0xB3, 
    0x99, 0x99, 0x0E, 0x30, 0x8E, 0x8A, 0x8F, 0xCD, 0x36, 0x36, 0xAF, 0x19, 0xD9, 0x81, 0x1D, 0xEF, 
    0x46, 0xB4, 0xC9, 0xA5, 0x70, 0xE7, 0x34, 0xC8, 0x57, 0x49, 0x49, 0x89, 0x1E, 0x03, 0x8B, 0x39, 
    0x99, 0x8E, 0xBF, 0xAA, 0x75, 0xAC, 0xC7, 0xDC, 0xE2, 0xCE, 0x66, 0x64, 0x38, 0xC2, 0x3A, 0x2A, 
    0x3F, 0x34, 0xD8, 0xDA, 0xBC, 0x67, 0x66, 0x04, 0x77, 0xBD, 0x1A, 0xD3, 0x26, 0x95, 0xC3, 0x9C, 
    0xD3, 0x21, 0x5D, 0x25, 0x26, 0x24, 0x78, 0x0E, 0x2C, 0xE6, 0x66, 0x3A, 0xFE, 0xA9, 0xD6, 0xB3, 
    0x1F, 0x73, 0x8B, 0x39, 0x99, 0x90, 0xE3, 0x08, 0xE8, 0xA8, 0xFC, 0xD3, 0x63, 0x6A, 0xF1, 0x9D, 
    0x98, 0x11, 0xDE, 0xF4, 0x6B, 0x4C, 0x9A, 0x57, 0x0E, 0x73, 0x4C, 0x85, 0x74, 0x94, 0x98, 0x91, 
    0xE0, 0x38, 0xB3, 0x99, 0x98, 0xEB, 0xFA, 0xA7, 0x5A, 0xCC, 0x7D, 0xCE, 0x2C, 0xE6, 0x66, 0x43, 
    0x8C, 0x2B, 0xA2, 0xA3, 0xFC, 0xE9, 0xB1, 0xB5, 0x78, 0xCE, 0xCC, 0x08, 0xEF, 0x7A, 0x35, 0xA6, 
    0x4D, 0x2B, 0x87, 0x39, 0xA6, 0x42, 0xBA, 0x4A, 0x4C, 0x48, 0xF0, 0x1C, 0x59, 0xCC, 0xCC, 0x75, 
    0xFF, 0x00, 0xFF, 0xC4, 0x00, 0x1B, 0x10, 0x00, 0x02, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x01, 0x31, 0x33, 0x20, 0x32, 0x10, 0xFF, 
    0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x05, 0x02, 0x19, 0x98, 0xCF, 0x04, 0xD7, 0x13, 0x63, 
    0xB5, 0x19, 0xA7, 0xC6, 0x66, 0x33, 0xC1, 0x35, 0xC4, 0xD8, 0xED, 0x46, 0x69, 0xF1, 0x99, 0x8C, 
    0xF0, 0x4D, 0x71, 0x36, 0x3B, 0x51, 0x9A, 0x7C, 0x66, 0x63, 0x3C, 0x13, 0x5C, 0x4D, 0x8E, 0xD4, 
    0x66, 0x9F, 0x19, 0x98, 0xCF, 0x04, 0xD7, 0x13, 0x63, 0xB5, 0x19, 0xA7, 0xC6, 0x66, 0x33, 0xC1, 
    0x35, 0xC4, 0xD8, 0xED, 0x46, 0x69, 0xF1, 0x99, 0x8C, 0xF0, 0x4D, 0x71, 0x36, 0x3B, 0x51, 0x9A, 
    0x7C, 0x66, 0x63, 0x3C, 0x13, 0x5C, 0x4D, 0x8E, 0xD4, 0x66, 0x9F, 0x19, 0x98, 0xCF, 0x04, 0xD7, 
    0x13, 0x63, 0xB5, 0x19, 0xA1, 0xFF, 0xC4, 0x00, 0x1F, 0x11, 0x00, 0x01, 0x02, 0x07, 0x01, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x10, 0x03, 0x04, 0x31, 
    0x32, 0x34, 0x72, 0xB1, 0x20, 0x00, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x03, 0x01, 0x01, 0x3F, 0x01, 
    0xF9, 0x17, 0x06, 0x9C, 0xC6, 0x8B, 0xA9, 0xE3, 0x0A, 0xF9, 0x14, 0x69, 0xFC, 0xB8, 0xDB, 0x1E, 
    0xB2, 0x2D, 0x0E, 0x8B, 0x83, 0x4E, 0x63, 0x45, 0xD4, 0xF1, 0x85, 0x7C, 0x8A, 0x34, 0xFE, 0x5C, 
    0x6D, 0x8F, 0x59, 0x16, 0x87, 0x45, 0xC1, 0xA7, 0x31, 0xA2, 0xEA, 0x78, 0xC2, 0xBE, 0x45, 0x1A, 
    0x7F, 0x2E, 0x36, 0xC7, 0xAC, 0x8B, 0x43, 0xA2, 0xE0, 0xD3, 0x98, 0xD1, 0x75, 0x3C, 0x61, 0x5F, 
    0x22, 0x8D, 0x3F, 0x97, 0x1B, 0x65, 0x75, 0x91, 0x68, 0x74, 0x5C, 0x1A, 0x73, 0x1A, 0x2E, 0xA7, 
    0x8C, 0x2B, 0xE4, 0x51, 0xA7, 0xF2, 0xE3, 0x6C, 0x7A, 0xC8, 0xB4, 0x3A, 0x2E, 0x0D, 0x39, 0x8D, 
    0x17, 0x53, 0xC6, 0x15, 0xF2, 0x28, 0xD3, 0xF9, 0x71, 0xB6, 0x57, 0x59, 0x16, 0x87, 0x45, 0xC1, 
    0xA7, 0x31, 0xA2, 0xEA, 0x78, 0xC2, 0xBE, 0x45, 0x1A, 0x7F, 0x2E, 0x36, 0xC7, 0xAC, 0x8B, 0x43, 
    0xA2, 0xE0, 0xD3, 0x98, 0xD1, 0x75, 0x3C, 0x61, 0x5F, 0x22, 0x8D, 0x3F, 0x97, 0x1B, 0x65, 0x75, 
    0x91, 0x68, 0x74, 0x5C, 0x1A, 0x73, 0x1A, 0x2E, 0xA7, 0x8C, 0x2B, 0xE4, 0x51, 0xA7, 0xF2, 0xE3, 
    0x6C, 0xAE, 0xB2, 0x2D, 0x0D, 0xFF, 0xC4, 0x00, 0x1E, 0x11, 0x00, 0x00, 0x07, 0x01, 0x01, 0x01, 
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x03, 0x05, 0x10, 0x34, 0x72, 
    0xB1, 0x33, 0x71, 0x20, 0x00, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x02, 0x01, 0x01, 0x3F, 0x01, 0xFC, 
    0xA3, 0x08, 0xEA, 0x8B, 0x19, 0x3E, 0x61, 0x36, 0x0E, 0xB1, 0xBC, 0xC5, 0xE7, 0xC9, 0xBD, 0x05, 
    0xEB, 0x27, 0x42, 0x26, 0xA1, 0xC6, 0x51, 0x9A, 0x75, 0x85, 0xAE, 0xA3, 0x08, 0xEA, 0x8B, 0x19, 
    0x3E, 0x61, 0x36, 0x0E, 0xB1, 0xBC, 0xC5, 0xE7, 0xC9, 0xBD, 0x05, 0xEB, 0x27, 0x42, 0x26, 0xA1, 
    0xC6, 0x51, 0x9A, 0x75, 0x85, 0xAE, 0xA3, 0x08, 0xEA, 0x8B, 0x19, 0x3E, 0x61, 0x36, 0x0E, 0xB1, 
    0xBC, 0xC5, 0xE7, 0xC9, 0xBD, 0x05, 0xEB, 0x27, 0x42, 0x26, 0xA1, 0xC6, 0x51, 0x9A, 0x75, 0x85, 
    0xAE, 0xA3, 0x08, 0xEA, 0x8B, 0x19, 0x3E, 0x61, 0x36, 0x0E, 0xB1, 0xBC, 0xC5, 0xE7, 0xC9, 0xBD, 
    0x05, 0xEB, 0x27, 0x42, 0x26, 0xA1, 0xC6, 0x51, 0x9A, 0x75, 0x85, 0xAE, 0xA3, 0x08, 0xEA, 0x8B, 
    0x19, 0x3E, 0x61, 0x36, 0x0E, 0xB1, 0xBC, 0xC5, 0xE7, 0xC9, 0xBD, 0x05, 0xEB, 0x27, 0x42, 0x26, 
    0xA1, 0xC6, 0x51, 0x9A, 0x75, 0x85, 0xAE, 0xA3, 0x08, 0xEA, 0x8B, 0x19, 0x3E, 0x61, 0x36, 0x0E, 
    0xB1, 0xBC, 0xC5, 0xE7, 0xC9, 0xBD, 0x05, 0xEB, 0x27, 0x42, 0x26, 0xA1, 0xC6, 0x51, 0x9A, 0x75, 
    0x85, 0xAE, 0xA3, 0x08, 0xEA, 0x8B, 0x19, 0x3E, 0x61, 0x36, 0x0E, 0xB1, 0xBC, 0xC5, 0xE7, 0xC9, 
    0xBD, 0x05, 0xEB, 0x27, 0x42, 0x26, 0xA1, 0xC6, 0x51, 0x9A, 0x75, 0x85, 0xAE, 0xA3, 0x08, 0xEA, 
    0x8B, 0x19, 0x3E, 0x61, 0x36, 0x0E, 0xB1, 0xBC, 0xC5, 0xE7, 0xC9, 0xBD, 0x05, 0xEB, 0x27, 0x42, 
    0x26, 0xA1, 0xC6, 0x51, 0x9A, 0x75, 0x85, 0xAE, 0xA3, 0x08, 0xEA, 0x8B, 0x19, 0x3E, 0x61, 0x36, 
    0x0E, 0xB1, 0xBC, 0xC5, 0xE7, 0xC9, 0xBD, 0x05, 0xEB, 0x27, 0x42, 0x26, 0xA1, 0xC6, 0x51, 0x9A, 
    0x75, 0x85, 0xAD, 0xFF, 0xC4, 0x00, 0x19, 0x10, 0x00, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x10, 0x71, 0x30, 0x01, 0x00, 0xFF, 0xDA, 
    0x00, 0x08, 0x01, 0x01, 0x00, 0x06, 0x3F, 0x02, 0xF1, 0x44, 0x53, 0x13, 0xA8, 0xAB, 0x28, 0x8A, 
    0x62, 0x77, 0xA8, 0xAB, 0x28, 0x8A, 0x62, 0x77, 0xA8, 0xAB, 0x28, 0x8A, 0x62, 0x75, 0x15, 0x65, 
    0x11, 0x4C, 0x4E, 0xA2, 0xAC, 0xA2, 0x29, 0x89, 0xD4, 0x55, 0x94, 0x45, 0x31, 0x3A, 0x8A, 0xB2, 
    0x88, 0xA6, 0x27, 0x51, 0x56, 0x51, 0x14, 0xC4, 0xEA, 0x2A, 0xBF, 0xFF, 0xC4, 0x00, 0x1C, 0x10, 
    0x01, 0x01, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x10, 0xA1, 0x31, 0x01, 0x71, 0xB1, 0x51, 0xF0, 0x00, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 
    0x01, 0x3F, 0x21, 0xFE, 0xA0, 0x54, 0x34, 0x78, 0xC3, 0x6E, 0xCA, 0xFE, 0x96, 0x9A, 0x05, 0x43, 
    0x47, 0x8C, 0x36, 0xEC, 0xFC, 0xEF, 0xA5, 0xA6, 0x81, 0x50, 0xD1, 0xE3, 0x0D, 0xBB, 0x3F, 0x3B, 
    0xE9, 0x69, 0xA0, 0x54, 0x34, 0x78, 0xC3, 0x6E, 0xCA, 0xFE, 0x96, 0x9A, 0x05, 0x43, 0x47, 0x8C, 
    0x36, 0xEC, 0xAF, 0xE9, 0x69, 0xA0, 0x54, 0x34, 0x78, 0xC3, 0x6E, 0xCA, 0xFE, 0x96, 0x9A, 0x05, 
    0x43, 0x47, 0x8C, 0x36, 0xEC, 0xAF, 0xE9, 0x69, 0xA0, 0x54, 0x34, 0x78, 0xC3, 0x6E, 0xCA, 0xFE, 
    0x96, 0x9A, 0x05, 0x43, 0x47, 0x8C, 0x36, 0xEC, 0xAF, 0xE9, 0x68, 0xFF, 0xDA, 0x00, 0x0C, 0x03, 
    0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x00, 0x00, 0x10, 0x67, 0xFB, 0xAD, 0x72, 0x36, 0xAC, 0x67, 
    0xFB, 0xAD, 0x72, 0x36, 0xAC, 0x67, 0xFB, 0xAD, 0x72, 0x36, 0xA4, 0x67, 0xFB, 0xAD, 0x72, 0x34, 
    0xAC, 0x67, 0xFB, 0xAD, 0x72, 0x34, 0xA4, 0x67, 0xFB, 0xAD, 0x72, 0x34, 0xAC, 0x67, 0xFB, 0xAD, 
    0x72, 0x34, 0xA4, 0x67, 0xFB, 0xAD, 0x72, 0x34, 0xAC, 0x67, 0xF3, 0xAD, 0x32, 0x34, 0xAC, 0xFF, 
    0xC4, 0x00, 0x1A, 0x11, 0x00, 0x02, 0x03, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x31, 0x51, 0xA1, 0xF0, 0x00, 0xFF, 0xDA, 0x00, 0x08, 0x01, 
    0x03, 0x01, 0x01, 0x3F, 0x10, 0xF6, 0x80, 0xBB, 0x36, 0x50, 0x31, 0x01, 0x44, 0xBB, 0x36, 0x58, 
    0x03, 0xD0, 0x17, 0x66, 0xCA, 0x06, 0x20, 0x28, 0x97, 0x66, 0xCB, 0x00, 0x7A, 0x02, 0xEC, 0xD9, 
    0x40, 0xC4, 0x05, 0x12, 0xEC, 0xD9, 0x60, 0x0F, 0x40, 0x5D, 0x9B, 0x28, 0x18, 0x80, 0xA2, 0x5D, 
    0x8B, 0x2C, 0x01, 0xE8, 0x0B, 0xB3, 0x65, 0x03, 0x10, 0x14, 0x4B, 0xB3, 0x65, 0x80, 0x3D, 0x01, 
    0x76, 0x6C, 0xA0, 0x62, 0x02, 0x89, 0x76, 0x2C, 0xB0, 0x07, 0xA0, 0x2E, 0xCD, 0x94, 0x0C, 0x40, 
    0x51, 0x2E, 0xCD, 0x96, 0x00, 0xF4, 0x05, 0xD9, 0xB2, 0x81, 0x88, 0x0A, 0x25, 0xD8, 0xB2, 0xC0, 
    0x1E, 0x80, 0xBB, 0x36, 0x50, 0x31, 0x0A, 0x25, 0xD8, 0xB2, 0xC0, 0x17, 0xFF, 0xC4, 0x00, 0x1B, 
    0x11, 0x00, 0x02, 0x02, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x10, 0x51, 0x00, 0xF0, 0x01, 0xA1, 0xB1, 0x31, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x02, 0x01, 
    0x01, 0x3F, 0x10, 0x97, 0xCE, 0x2E, 0x96, 0x66, 0x6E, 0x38, 0x71, 0xE0, 0xDC, 0x74, 0x5B, 0x28, 
    0xB6, 0x73, 0x7C, 0xE2, 0xE9, 0x66, 0x66, 0xE3, 0x87, 0x1E, 0x0D, 0xC7, 0x45, 0xB2, 0x8B, 0x67, 
    0x37, 0xCE, 0x2E, 0x96, 0x66, 0x6E, 0x38, 0x71, 0xE0, 0xDC, 0x74, 0x5B, 0x28, 0xB6, 0x73, 0x7C, 
    0xE2, 0xE9, 0x66, 0x66, 0xE3, 0x87, 0x1E, 0x0D, 0xC7, 0x45, 0xB2, 0x8B, 0xE7, 0x37, 0xCE, 0x2E, 
    0x96, 0x66, 0x6E, 0x38, 0x71, 0xE0, 0xDC, 0x74, 0x5B, 0x28, 0xB6, 0x73, 0x7C, 0xE2, 0xE9, 0x66, 
    0x66, 0xE3, 0x87, 0x1E, 0x0D, 0xC7, 0x45, 0xB2, 0x8B, 0xE7, 0x37, 0xCE, 0x2E, 0x96, 0x66, 0x6E, 
    0x38, 0x71, 0xE0, 0xDC, 0x74, 0x5B, 0x28, 0xB6, 0x73, 0x7C, 0xE2, 0xE9, 0x66, 0x66, 0xE3, 0x87, 
    0x1E, 0x0D, 0xC7, 0x45, 0xB2, 0x8B, 0xE7, 0x37, 0xCE, 0x2E, 0x96, 0x66, 0x6E, 0x38, 0x71, 0xE0, 
    0xDC, 0x74, 0x5B, 0x28, 0xB6, 0x71, 0xFF, 0xC4, 0x00, 0x1B, 0x10, 0x00, 0x02, 0x02, 0x03, 0x01, 
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x51, 0xF0, 0x00, 
    0x31, 0xA1, 0x71, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x01, 0x3F, 0x10, 0xCB, 0x68, 0x2A, 
    0xBA, 0x0A, 0xE7, 0x2F, 0x47, 0x8B, 0xAD, 0x87, 0x59, 0x77, 0x2C, 0x1B, 0xCA, 0xB9, 0x2E, 0xDA, 
    0x0A, 0xAE, 0x82, 0xB9, 0xCB, 0xD1, 0xE2, 0xEB, 0x61, 0xD6, 0x5D, 0x4B, 0x06, 0xF2, 0xAE, 0x4B, 
    0xB6, 0x82, 0xAB, 0xA0, 0xAE, 0x72, 0xF4, 0x78, 0xBA, 0xD8, 0x75, 0x97, 0x52, 0xC1, 0xBC, 0xAB, 
    0x92, 0xED, 0xA0, 0xAA, 0xE8, 0x2B, 0x9C, 0xBD, 0x1E, 0x2E, 0xB2, 0xAB, 0x96, 0x0D, 0xE5, 0x5C, 
    0x97, 0x6D, 0x05, 0x57, 0x41, 0x5C, 0xE5, 0xE8, 0xF1, 0x75, 0x95, 0x5C, 0xB0, 0x6F, 0x2A, 0xE4, 
    0xBB, 0x68, 0x2A, 0xBA, 0x0A, 0xE7, 0x2F, 0x47, 0x8B, 0xAC, 0xAA, 0xE5, 0x83, 0x79, 0x57, 0x25, 
    0xDB, 0x41, 0x55, 0xD0, 0x57, 0x39, 0x7A, 0x3C, 0x5D, 0x65, 0x57, 0x2C, 0x1B, 0xCA, 0xB9, 0x2E, 
    0xDA, 0x0A, 0xAE, 0x82, 0xB9, 0xCB, 0xD1, 0xE2, 0xEB, 0x2A, 0xB9, 0x60, 0xDE, 0x55, 0xC9, 0x76, 
    0xD0, 0x55, 0x74, 0x15, 0xCE, 0x5E, 0x8F, 0x17, 0x59, 0x5D, 0xCB, 0x06, 0xF2, 0xAE, 0x4A, 0xFF, 
0xD9, 
} ;

int liveViewHandle;

static void ev_handler(struct mg_connection *c, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    struct http_message *hm = (struct http_message *) p;
    char reply[100];

	//WPRINTF("URI %d",hm->uri.len);

	char URI[100];
	char QString[100];


	strncpy(URI, hm->uri.p, hm->uri.len);
	URI[hm->uri.len] = '\0';

	WPRINTF("\n URI %s",URI);

	strncpy(QString, hm->query_string.p, hm->query_string.len);
	QString[hm->query_string.len] = '\0';

	WPRINTF("\n query_string %s",QString);

	if(strcmp("/",URI) == 0)
	{
		WPRINTF("\n Index");
		
		mg_printf(c,
	              "HTTP/1.1 200 OK\r\n"
	              "Content-Type: text/html\r\n"
	              "Content-Length: %d\r\n"
	              "\r\n"
	              "%s",
	              SourceInterfaceSize, SourceInterface);
	}
	else if(strcmp("/Settings.html",URI) == 0)
	{
		WPRINTF("\n Settings.html");
		
		mg_printf(c,
	              "HTTP/1.1 200 OK\r\n"
	              "Content-Type: text/html\r\n"
	              "Content-Length: %d\r\n"
	              "\r\n"
	              "%s",
	              SourceSettingsSize, SourceSettings);
	}
	else if(strcmp("/P2PStatus",URI) == 0)
	{
		char configData[4096];
		int i;
		int messagefromlocalhost = 0;
		
		for(i=0; i < MG_MAX_HTTP_HEADERS; i++)
		{
			char hdr[128] = {0};
			char val[128] = {0};
			int len;

			len = hm->header_names[i].len;
			if(len > 127)
				len = 127;
			strncpy(hdr, hm->header_names[i].p, len);

			len = hm->header_values[i].len;
			if(len > 127)
				len = 127;
			strncpy(val, hm->header_values[i].p, len);

			//if(strcmp(hdr,"Host") == 0)
			{
				//if(strstr(val,"127.0.0.1") != NULL)
				{
					messagefromlocalhost = 1;
					break;
				}
			}
		}

		if(messagefromlocalhost == 1)
		{
			strncpy(configData,hm->body.p,hm->body.len);
			configData[hm->body.len] = '\0';

			SetLogs2(configData);
		}
	}
	else if(strcmp("/ConfigValue",URI) == 0)
	{
		char configs[2048];
		WPRINTF("\n ConfigValue");

		GetConfiguration(configs);
		
		mg_printf(c,
	              "HTTP/1.1 200 OK\r\n"
	              "Content-Type: text/html\r\n"
	              "Content-Length: %d\r\n"
	              "\r\n"
	              "%s",
	              strlen(configs), configs);
	}
	else if(strcmp("/LogValue",URI) == 0)
	{
		char logs[LOG_BUFFER_SIZE];
		WPRINTF("\n LogValue");

		GetLogs(logs);
		
		mg_printf(c,
	              "HTTP/1.1 200 OK\r\n"
	              "Content-Type: text/html\r\n"
	              "Content-Length: %d\r\n"
	              "\r\n"
	              "%s",
	              strlen(logs), logs);
	}
	else if(strcmp("/LogValue2",URI) == 0)
	{
		char logs[4096];
		WPRINTF("\n LogValue2");

		GetLogs2(logs);
		
		mg_printf(c,
	              "HTTP/1.1 200 OK\r\n"
	              "Content-Type: text/html\r\n"
	              "Content-Length: %d\r\n"
	              "\r\n"
	              "%s",
	              strlen(logs), logs);
	}
	else if(strcmp("/SaveConfig",URI) == 0)
	{
		char configData[2048];
		char *pch=NULL;
		int pchCount = 0;
		int prevHTTPPort, currHTTPPort;
		
		WPRINTF("\n SaveConfig");

		//const char *contentLength = mg_get_http_header(hm, "Content-Length");
		
		strncpy(configData,hm->body.p,hm->body.len);
		configData[hm->body.len] = '\0';
		
		setAppStatus(0);

		prevHTTPPort = GetHTTPPort();
		
		//printf("\n Configuration Data \n");

		pch= strtok (configData,"*");
		while (pch != NULL)
		{
			//printf("%s %d\n",pch,pchCount);

			SetConfiguration(pch,pchCount);
			
			pch = strtok (NULL, "*");
			
			pchCount++;
		}

		SaveConfiguration();

		SetLogs2("");

		currHTTPPort = GetHTTPPort();

		if(currHTTPPort != prevHTTPPort)
		{
			restartApplicationVar = 2;

			mg_printf(c,
		              "HTTP/1.1 200 OK\r\n"
		              "Content-Type: text/html\r\n"
		              "Content-Length: %d\r\n"
		              "\r\n"
		              "%s",
		              strlen("restart"), "restart");
		}
		else
		{
			restartApplicationVar = 1;

			mg_printf(c,
		              "HTTP/1.1 200 OK\r\n"
		              "Content-Type: text/html\r\n"
		              "Content-Length: 0\r\n"
		              "\r\n"
		             );
		}
	}
	else if(strcmp("/StartStop",URI) == 0)
	{
		char value[1024];
		char *pch=NULL;
		int pchCount = 0;
		
		WPRINTF("\n StartStop");

		strncpy(value,hm->body.p,hm->body.len);
		value[hm->body.len] = '\0';

		//printf("\n Value : %s",value);

		if(strcmp(value,"Start") == 0)
			setAppStatus(1);
		else
			setAppStatus(0);
		
		mg_printf(c,
	              "HTTP/1.1 200 OK\r\n"
	              "Content-Type: image/jpeg\r\n"
	              "Content-Length: 0\r\n"
	              "\r\n"
	             );
	}
	else if(strcmp("/liveFeed",URI) == 0)
	{
		WPRINTF("\n liveFeed");

		if(getAppStatus())
		{
			liveImageBuffer_Size = get_YUV2BMP_data(liveViewHandle, liveImageBuffer);

			#if 0
			{
			char filename[100];
			static int FileCount = 1;
			sprintf(filename,"T%05d.bmp", FileCount);
			printf("\nliveFeed :: File Name %s",filename);
			FILE * tfptr = fopen(filename, "wb");
			fwrite(liveImageBuffer, 1, liveImageBuffer_Size, tfptr);
			fclose(tfptr);
			FileCount++;
			}
			#endif
			
			mg_printf(c,
		              "HTTP/1.1 200 OK\r\n"
		              "Content-Type:  image/bmp\r\n"
		              "Content-Length: %d\r\n"
		              "Access-Control-Allow-Origin: *\r\n"
			"Connection: keep-alive\r\n"
			"Cache-Control: private, max-age=1\r\n"
		              "\r\n",
		              liveImageBuffer_Size);

			mg_send(c, liveImageBuffer, liveImageBuffer_Size);
			
		}
		else
		{
			mg_printf(c,
		              "HTTP/1.1 200 OK\r\n"
		              "Content-Type:  image/jpeg\r\n"
		              "Content-Length: %d\r\n"
		              "Access-Control-Allow-Origin: *\r\n"
			"Connection: keep-alive\r\n"
			"Cache-Control: private, max-age=1\r\n"
		              "\r\n",
		              sizeof(TV));

			mg_send(c, TV, sizeof(TV));
		}

		
	}
	else
	{
		mg_printf(c,
	              "HTTP/1.1 200 OK\r\n"
	              "Content-Type: image/jpeg\r\n"
	              "Content-Length: 0\r\n"
	              "\r\n"
	             );
	}
  }
}

int readHTML(char * filename, char * buf)
{
  	FILE 	* fptr 	= NULL;
	int 		count =0;
	
	if( (fptr  = fopen( filename, "rb" )) == NULL )
	{
      		printf( "readHTML file '%s' is not opened\n", filename );
		exit(-1);
	}       
	
	while(!feof(fptr))
	{		
		count += fread( buf, sizeof( char ), 1, fptr );
		buf++;
		if (count > MAX_HTML_PAGE_SIZE)
		{
			printf("ERROR: readHTML Memory Not Enough ");
			exit(-1);
		}
	}

	fclose(fptr);
	
	WPRINTF("readHTML Size %d  \n", count);

	return count;

}

int WebServerThread(void * arg)
{
	pWebServerSettings_t 	pSettings = (pWebServerSettings_t ) arg;
	struct mg_mgr mgr;
	struct mg_connection *nc;
	liveViewHandle = pSettings->view_handle;
	
	printf("WebServerThread : httpPort : %s",pSettings->httpPort);
	mg_mgr_init(&mgr, NULL);
	nc = mg_bind(&mgr, pSettings->httpPort, ev_handler);
	mg_set_protocol_http_websocket(nc);


	/* Read Pages */

	SourceInterfaceSize = readHTML("html/source.html",SourceInterface);
	SourceSettingsSize = readHTML("html/sourceSettings.html",SourceSettings);

	
	/* For each new connection, execute ev_handler in a separate thread */
	//mg_enable_multithreading(nc);

	pSettings->threadStatus = RUNNING_THREAD;
	printf("Starting multi-threaded server on port %s\n", pSettings->httpPort);
	//for (;;) 
	while(pSettings->threadStatus == RUNNING_THREAD)
	{
		if(pSettings->pause == TRUE_STATE)
			break;
		
		mg_mgr_poll(&mgr, 3000);
	}
	mg_mgr_free(&mgr);

	printf("Web Server Break ...\n");

	pSettings->threadStatus = STOPPED_THREAD;

}
