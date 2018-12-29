/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// Sav2CartLZ4.cpp

#ifdef _MSC_VER
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <stdint.h>


//////////////////////////////////////////////////////////////////////
// LZ4 packer/unpacker

#include "smallz4.h"

// ==================== I/O INTERFACE ====================

unsigned char *indata  = NULL;
unsigned char *outdata = NULL;

size_t  inpos = 0,  inposmax = 0;
size_t outpos = 0, outposmax = 0, skip_counter = 0;

/// read a block of bytes
size_t getBytesFromIn(void* data, size_t numBytes)
{
    if (data && numBytes > 0)
    {
        if ((inpos + numBytes) > inposmax) numBytes = (inposmax - inpos);

        ::memcpy(data, &indata[inpos], numBytes);
        inpos += numBytes;
        return numBytes;
    }
    return 0;
}

/// write a block of bytes
void sendBytesToOut(const void* data, size_t numBytes)
{
    // Skip header
    if(numBytes > skip_counter) {
        numBytes -= skip_counter;
        if (data && numBytes > 0)
        {
            if ((outpos + numBytes) > outposmax) numBytes = (outposmax - outpos);

            ::memcpy(&outdata[outpos], (const char *)data + skip_counter, numBytes);
            outpos += numBytes;
            skip_counter=0;
        }
    } else {
        skip_counter -= numBytes;
        return;
    }
}

// ======================================================

size_t lz4_encode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize)
{
    inpos = 0;
    indata = inbuffer;
    inposmax = insize;

    skip_counter = 13; // Skip header
    outpos = 0;
    outdata = outbuffer;
    outposmax = outsize;

    smallz4::lz4(getBytesFromIn, sendBytesToOut, 65536);

    printf("LZ4 input size %lu. bytes\n", insize);
    printf("LZ4 output size %lu. bytes (%1.2f %%)\n", outpos, outpos * 100.0 / insize);
    return outpos;
}

size_t lz4_decode(unsigned char *inbuffer, size_t insize, unsigned char *outbuffer, size_t outsize)
{
    return 0;
}


//////////////////////////////////////////////////////////////////////


static uint16_t const loaderLZ4[] =
{
    0000240,  // 000000  000240  NOP
    0012702,  // 000002  012702  MOV     #000104, R2    ; Адрес массива параметров
    0000104,
    0110062,  // 000006  110062  MOVB    R0, 000003(R2)
    0000003,
    0012701,  // 000012  012701  MOV     #000005, R1
    0000005,
    0012703,  // 000016  012703  MOV     #000116, R3
    0000116,
    0000402,  // 000022  000402  BR      000030
    0112337,  // 000024  112337  MOVB    (R3)+, @#176676
    0176676,
    0105737,  // 000030  105737  TSTB    @#176674
    0176674,
    0100375,  // 000034  100375  BPL     000030
    0077106,  // 000036  077106  SOB     R1, 000024
    0105712,  // 000040  105712  TSTB    (R2)
    0001356,  // 000042  001356  BNE     000000
    // Подсчёт контрольной суммы
    0005003,  // 000044  005003  CLR     R3
    0012701,  // 000046  012701  MOV     #100600, R1
    0100600,  // 000050  100600
    0012702,  // 000052  012702  MOV     #027400, R2
    0027400,  // 000054  027400
    0062103,  // 000056  062103  ADD     (R1)+, R3
    0005503,  // 000060  005503  ADC     R3
    0077203,  // 000062  077203  SOB     R2, 000056
    0020327,  // 000064  020327  CMP     R3, #CHKSUM
    0000000,  // 000066  ?????? <= CHKSUM
    0001343,  // 000070  001343  BNE     000000
    0000413,  // 000072  000413  BR      000122        ; Переход на LZ4 unpacker
    // Запуск загруженной программы на выполнение
    0012706,  // 000074  016706  MOV	#STACK, SP
    0001000,  // 000076  ?????? <= STACK
    0000137,  // 000100  000137  JMP    START   ; Переход на загруженный код
    0001000,  // 000102  ?????? <= START
    // Массив параметров для получения данных с кассеты ПЗУ через канал 2
    0004000,  // 000104  004000   ; Команда (10) и ответ
    0000021,  // 000106  000021   ; Номер кассеты и номер устройства
    0001000,  // 000110  001000   ; Адрес от начала кассеты ПЗУ
    0100600,  // 000112  100600   ; Адрес в ОЗУ == LZ4TA
    0027400,  // 000114  027400   ; Количество слов = 12032. слов = 24064. байт
    0000104,  // 000116
    0177777,  // 000120
                      // LZ4 unpacker for PDP11/EIS by Alexander Troosh
    0012705, 0001000, // 000122  012705 ??????'          MOV     #dst, R5
    0012700, 0000000, // 000126  012700 ??????'          MOV     #src, R0
    0012704, 0177774, // 000132  012704 177774           MOV     #-4,  R4        ; Нет лучше места под константу -4, чем R4
                      //
    0005002,          // 000136  005002                  CLR     R2
                      // 000140                  gettoken:
    0152002,          // 000140  152002                  BISB    (R0)+, R2       ; (на входе всегда R2=0), считываем
    0010201,          // 000142  010201                  MOV     R2, R1          ; байт-токен без знакового расширения
                      //
    0072104,          // 000144  072104                  ASH     R4, R1          ; Старший полубайт - число литералов
    0001412,          // 000146  001412                  BEQ     noliterals      ; Литералов может и не быть...
    0022701, 0000017, // 000150  022701 000017           CMP     #^X0f, R1       ; Признак большой длины?
    0001005,          // 000154  001005                  BNE     copylits
    0005003,          // 000156  005003                    CLR     R3
    0152003,          // 000160  152003          1$:         BISB  (R0)+, R3     ; Уточняем длину...
    0060301,          // 000162  060301                      ADD   R3, R1
    0105203,          // 000164  105203                      INCB  R3            ; бесконечно долго, пока приходят 0xFF
    0001774,          // 000166  001774                      BEQ   1$
                      //
    0112025,          // 000170  112025          copylits: MOVB  (R0)+, (R5)+    ; Копируем литералы
    0077102,          // 000172  077102                    SOB     R1, copylits
                      //
                      // 000174                  noliterals:
    0012003,          // 000174  012003                  MOV     (R0)+, R3       ; Получаем два байт смещения
    0001702,          // 000176  001702                  BEQ     LAUNCH          ; Нулевое смещение - конец сжатого блока
                      //                                                         ; R1=0, как бы мы сюда не попали
    0042702, 0177760, // 000200  042702 177760           BIC     #^X0fff0, R2    ; Младший полубайт - число копируемых байт
    0022702, 0000017, // 000204  022702 000017           CMP     #^X0f, R2       ; Признак большой длины?
    0001004,          // 000210  001004                  BNE     shortstr
    0152001,          // 000212  152001          2$:       BISB  (R0)+, R1       ; Уточняем длину...
    0060102,          // 000214  060102                    ADD   R1, R2
    0105201,          // 000216  105201                    INCB  R1              ; бесконечно долго, пока приходят 0xFF
    0001774,          // 000220  001774                    BEQ   2$
                      // 000222                  shortstr:
    0160402,          // 000222  160402                  SUB     R4, R2          ; Минимальный размер строки - 4 байта
    0010501,          // 000224  010501                  MOV     R5, R1
    0160301,          // 000226  160301                  SUB     R3, R1
    0112125,          // 000230  112125          copystr:  MOVB  (R1)+, (R5)+    ; Копируем строку из
    0077202,          // 000232  077202                    SOB   R2, copystr     ; уже распакованных данных
    0000741           // 000234  000741                  BR      gettoken
};


//////////////////////////////////////////////////////////////////////


char inputfilename[256];
char outputfilename[256];
FILE* inputfile;
FILE* outputfile;
uint8_t* pFileImage = NULL;
uint8_t* pCartImage = NULL;
uint16_t wStartAddr;
uint16_t wStackAddr;
uint16_t wTopAddr;

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Usage: Sav2CartLZ4 <inputfile.SAV> <outputfile.BIN>\n");
        return 255;
    }

    strcpy(inputfilename, argv[1]);
    strcpy(outputfilename, argv[2]);

    printf("Input file: %s\n", inputfilename);

    inputfile = fopen(inputfilename, "rb");
    if (inputfile == nullptr)
    {
        printf("Failed to open the input file (%d).", errno);
        return 255;
    }
    ::fseek(inputfile, 0, SEEK_END);
    uint32_t inputfileSize = ::ftell(inputfile);

    pFileImage = (uint8_t*) ::malloc(inputfileSize);

    ::fseek(inputfile, 0, SEEK_SET);
    size_t bytesRead = ::fread(pFileImage, 1, inputfileSize, inputfile);
    if (bytesRead != inputfileSize)
    {
        printf("Failed to read the input file.");
        return 255;
    }
    ::fclose(inputfile);
    printf("Input file size %u. bytes\n", inputfileSize);

    wStartAddr = *((uint16_t*)(pFileImage + 040));
    wStackAddr = *((uint16_t*)(pFileImage + 042));
    wTopAddr = *((uint16_t*)(pFileImage + 050));
    printf("SAV Start\t%06ho  %04x  %5d\n", wStartAddr, wStartAddr, wStartAddr);
    printf("SAV Stack\t%06ho  %04x  %5d\n", wStackAddr, wStackAddr, wStackAddr);
    printf("SAV Top  \t%06ho  %04x  %5d\n", wTopAddr, wTopAddr, wTopAddr);
    size_t savImageSize = ((size_t)wTopAddr + 2 - 01000);
    printf("SAV image size\t%06ho  %04lx  %5lu\n", (uint16_t)savImageSize, savImageSize, savImageSize);

    pCartImage = (uint8_t*) ::calloc(65536, 1);
    if (pCartImage == NULL)
    {
        printf("Failed to allocate memory.");
        return 255;
    }

    for (;;)
    {
        ::memset(pCartImage, -1, 65536);
        size_t lz4CodedSize = lz4_encode(pFileImage + 512, savImageSize, pCartImage + 512, 65536 - 512);
        if (lz4CodedSize > 24576 - 512)
        {
            printf("LZ4 encoded size too big: %lu. bytes, max %d. bytes\n", lz4CodedSize, 24576 - 512);
        }
        else
        {
#if 0
            // Trying to decode to make sure encoder works fine
            uint8_t* pTempBuffer = (uint8_t*) ::calloc(65536, 1);
            if (pTempBuffer == NULL)
            {
                printf("Failed to allocate memory.");
                return 255;
            }
            size_t decodedSize = lz4_decode(pCartImage + 512, lz4CodedSize, pTempBuffer, 65536);
            for (size_t offset = 0; offset < savImageSize; offset++)
            {
                if (pTempBuffer[offset] == pFileImage[512 + offset])
                    continue;

                printf("LZ4 decode failed at offset %06ho 0x%04x (%02x != %02x)\n", (uint16_t)(512 + offset), (uint16_t)(512 + offset), pTempBuffer[offset], pFileImage[512 + offset]);
                return 255;
            }
            ::free(pTempBuffer);
            printf("LZ4 decode check done, decoded size %lu. bytes\n", decodedSize);
#endif
            ::memcpy(pCartImage, pFileImage, 512);

            // Prepare the loader
            memcpy(pCartImage, loaderLZ4, sizeof(loaderLZ4));
            *((uint16_t*)(pCartImage + 0076)) = wStackAddr;
            *((uint16_t*)(pCartImage + 0102)) = wStartAddr;
            *((uint16_t*)(pCartImage + 0130)) = wStartAddr;
            break;  // Finished encoding with LZ4
        }

        return 255;  // All attempts failed
    }

    // Calculate checksum
    uint16_t* pData = ((uint16_t*)(pCartImage + 01000));
    uint16_t wChecksum = 0;
    for (int i = 0; i < 027400; i++)
    {
        uint16_t src = wChecksum;
        uint16_t src2 = *pData;
        wChecksum += src2;
        if (((src & src2) | ((src ^ src2) & ~wChecksum)) & 0100000)  // if Carry
            wChecksum++;
        pData++;
    }
    *((uint16_t*)(pCartImage + 0066)) = wChecksum;

    ::free(pFileImage);

    printf("Output file: %s\n", outputfilename);
    outputfile = fopen(outputfilename, "wb");
    if (outputfile == nullptr)
    {
        printf("Failed to open output file (%d).", errno);
        return 255;
    }

    size_t bytesWrite = ::fwrite(pCartImage, 1, 24576, outputfile);
    if (bytesWrite != 24576)
    {
        printf("Failed to write to the output file.");
        return 255;
    }
    ::fclose(outputfile);

    ::free(pCartImage);

    printf("Done.\n");
    return 0;
}


//////////////////////////////////////////////////////////////////////
