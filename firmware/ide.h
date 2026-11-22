#ifndef IDE_H
#define IDE_H
/*
 * IDE emulation
 * 
 * Copyright (c) 2003-2016 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

typedef struct BlockDevice BlockDevice;

typedef struct IDEIFState IDEIFState;

IDEIFState *ide_allocate(int irq, void *pic, void (*set_irq)(void *pic, int irq, int level));
extern int ide_hda_rw;   /* 1 = write the image file in place (PC default, ini hda_rw=1); 0 = read-only + RAM overlay (board default) */
int ide_attach(IDEIFState *s, int drive, const char *filename);
int ide_attach_cd(IDEIFState *s, int drive, const char *filename);
void ide_change_cd(IDEIFState *sif, int drive, const char *filename);
void ide_reset(IDEIFState *s);

void ide_data_writew(void *opaque, uint32_t val);
uint32_t ide_data_readw(void *opaque);

void ide_data_writel(void *opaque, uint32_t val);
uint32_t ide_data_readl(void *opaque);

int ide_data_write_string(void *opaque, uint8_t *buf, int size, int count);
int ide_data_read_string(void *opaque, uint8_t *buf, int size, int count);

void ide_ioport_write(void *opaque, uint32_t offset, uint32_t val);
uint32_t ide_ioport_read(void *opaque, uint32_t offset);

void ide_cmd_write(void *opaque, uint32_t val);
uint32_t ide_status_read(void *opaque);

#include "pci.h"
PCIDevice *piix3_ide_init(PCIBus *pci_bus, int devfn);

int ide_dev_info(IDEIFState *s, int drive, int *cyl, int *heads, int *secs, int64_t *nb);
int ide_dev_read(IDEIFState *s, int drive, uint64_t lba, int n, uint8_t *buf);
int ide_dev_write(IDEIFState *s, int drive, uint64_t lba, int n, const uint8_t *buf);
void ide_fill_cmos(IDEIFState *s, void *cmos,
                   uint8_t (*set)(void *cmos, int addr, uint8_t val));
#endif /* IDE_H */
