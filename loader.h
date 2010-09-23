/*
 * Ricoh 5U87x series USB firmware loader.
 * Copyright (c) 2008 Alexander Hixon <alex@alexhixon.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA
 */

#include <glib.h>

#ifndef _LOADER_H_
#define _LOADER_H_

#define TIMEOUT 1000

#define loader_msg(args...) printf(args)
#define loader_warn(args...) ({printf("Warning: "); printf(args);})
#define loader_error(args...) ({printf("\nError: "); printf(args); exit(-1);})

#define USB_SEND USB_ENDPOINT_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE
#define USB_RECV USB_ENDPOINT_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE

struct device_info {
    int usb_vendor;
    int usb_product;
    int ucode_version;
    int hflip;
    int vflip;
};

struct usb_device*
find_device (const struct device_info devices[],
	     const struct device_info **info);

/*
 * Please try to keep this in sync with docs/model_matrix.txt and
 * docs/firmware_matrix.txt
 */
static const struct device_info device_table[] = {
    { 0x05CA, 0x1803, 0xFFFF, 0, 0 }, // Unknown ucode version.
    { 0x05CA, 0x1810, 0x0115, 0, 0 },
    /* 05ca:1812 does not require ucode. */
    { 0x05CA, 0x1830, 0x0100, 0, 0 },
    { 0x05CA, 0x1832, 0x0100, 0, 0 },
    { 0x05CA, 0x1833, 0x0100, 0, 0 },
    { 0x05CA, 0x1834, 0x0111, 0, 0 },
    { 0x05CA, 0x1835, 0x0107, 0, 0 },
    { 0x05CA, 0x1836, 0x0115, 0, 0 },
    { 0x05CA, 0x1837, 0x0115, 1, 1 }, // Uses copy of 0x1836 ucode.
    { 0x05CA, 0x1839, 0x0030, 0, 0 },
    { 0x05CA, 0x183a, 0x0111, 0, 0 },
    { 0x05CA, 0x183b, 0x0131, 0, 0 },
    { 0x05CA, 0x183e, 0x0100, 0, 0 },
    { 0x05CA, 0x1841, 0x0103, 1, 1 },
    
    /*
     * These have been commented out because we don't have the routines
     * to determine which is which yet.
     * 
     * { 0x05CA, 0x1870, 0x0100, 0, 0 },  // Used for HP Webcam 1000      (1870_1.fw)
     * { 0x05CA, 0x1870, 0x0112, 0, 0 },  // Used for HP Pavilion dv1xxxx (1870.fw)
     */
     
     { }
};

#endif
