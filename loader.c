/*
 * Ricoh R5U87x series USB firmware loader.
 * Copyright (c) 2008-2009 Alexander Hixon <alex@alexhixon.com>
 * 
 * Loading routines based off those used in the original r5u870 kernel
 * driver, written by Sam Revitch <samr7@cs.washington.edu>. See README
 * for additional credits.
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

#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <usb.h>

#include "loader.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define R5U870_REG_VFLIP_EX	0x30
#define R5U870_REG_HFLIP_EX	0x31

static gchar    *firmware       = "ucode/r5u87x-%vid%-%pid%.fw";	// relative path to firmware files
                                                                    // if files are not found here, loader will look
                                                                    // in UCODE_PATH for files - defined in Makefile
                                                                    // and config.h
static gboolean force_clear     = FALSE;
static gboolean no_load         = FALSE;

static gchar    *dump;
static gboolean dump_ucode      = FALSE;
static gboolean reload          = FALSE;

static GOptionEntry entries[] = {
    { "firmware", 'f', 0,
        G_OPTION_ARG_FILENAME, &firmware,
        "Path to microcode. %vid% and %pid% are substituted in.", NULL
    },
    { "force-clear", 0, 0,
        G_OPTION_ARG_NONE, &force_clear,
        "Forcefully clear the device before uploading microcode.", NULL
    },
    { "pretend", 0, 0,
        G_OPTION_ARG_NONE, &no_load,
        "Don't actually load any microcode on to the device.", NULL
    },
    { "dump-ucode", 0, 0,
        G_OPTION_ARG_NONE, &dump_ucode,
        "Dump microcode to compiled code.", NULL
    },
#ifdef ENABLE_RELOAD
    { "reload", 0, 0,
        G_OPTION_ARG_NONE, &reload,
        "Reload uvcvideo module when complete.", NULL
    },
#endif
};

gchar *
replace_str(gchar *str, gchar *orig, gchar *rep)
{
  static char buffer[4096];
  char *p;

  if(!(p = strstr(str, orig)))  // Is 'orig' even in 'str'?
    return str;

  strncpy(buffer, str, p-str); // Copy characters from 'str' start to 'orig'
  buffer[p-str] = '\0';

  sprintf(buffer+(p-str), "%s%s", rep, p+strlen(orig));

  return buffer;
}

struct usb_device *
find_device (const struct device_info devices[],
	     const struct device_info **info) {
    const struct device_info *devinfo;
    gint i = 0;
    struct usb_bus *busses;
    struct usb_bus *bus;
    
    busses = usb_get_busses();
    for (bus = busses; bus; bus = bus->next) {
        struct usb_device *dev;
        
        for (dev = bus->devices; dev; dev = dev->next) {
            do {
                devinfo = &devices[i];
                if (dev->descriptor.idVendor == devinfo->usb_vendor &&
                    dev->descriptor.idProduct == devinfo->usb_product &&
                    dev->descriptor.idVendor != 0) {
                    
		    *info = devinfo;
                    return dev;
                }
                
                i++;
            } while (devinfo->usb_vendor != 0);
            
            i = 0;
        }
    }
    
    return NULL;
}

gint
r5u87x_image_flip(struct usb_dev_handle *handle, gboolean hflip,
		  gboolean vflip) {
    gint res;

    res = usb_control_msg (handle, USB_SEND, 0xc2, hflip, R5U870_REG_HFLIP_EX,
                NULL, 0, TIMEOUT);
    if (res < 0) {
        loader_warn ("Unable to set hflip to %u; res: %d\n", hflip, res);
        return res;
    }

    res = usb_control_msg (handle, USB_SEND, 0xc2, vflip, R5U870_REG_VFLIP_EX,
                NULL, 0, TIMEOUT);
    if (res < 0) {
        loader_warn ("Unable to set vflip to %u; res: %d\n", vflip, res);
        return res;
    }

    return 0;
}

gint
r5u87x_ucode_upload (gint firmware, struct usb_dev_handle *handle, gint size) {
    gint length, remaining, address, index, res;
    unsigned char header[3], payload[1024];
    
    gint dump_fd;
    
    index = 0;
    remaining = size;
    
    loader_msg ("Sending microcode to camera...\n");
    
    if (dump_ucode) {
        loader_msg ("Dumping microcode to %s\n", dump);
        if ((dump_fd = g_open (dump, O_WRONLY | O_CREAT, 0666)) == -1) {
            loader_error ("Failed to open microcode dump.\n");
        }
    }
    
    while (remaining > 0) {
        if (remaining < 3) {
            loader_warn ("Microcode file is incomplete; message %d\n", index);
            return -EBADMSG;
        }
        
        // load in packet header
        if ((length = read (firmware, header, 3)) != 3) {
            if (length == 0) {
                return 0;
            } else {
                loader_warn ("Reading firmware header chunk failed\n");
                return -EBADMSG;
            }
        }
        
        length = header[0];
        address = header[1] | (header[2] << 8);
        remaining -= 3;
        
        // Read the payload
        if (length < 0 || length >= 1024) {
            loader_warn ("Bad payload length %d (%i).\n", length, index);
            return -EBADMSG;
        } else if (read (firmware, payload, length) != length) {
            loader_warn ("Failed to read firmware data.\n");
            return -EBADMSG;
        }
        
        // If we're dumping, just write the payload
        if (dump_ucode) {
            loader_msg ("Wrote %d bytes in message %d\n", length, index);
            write (dump_fd, payload, length);
        }
        
        if (no_load == FALSE) {
            res = usb_control_msg (handle, USB_SEND, 0xa0, address, 0,
                (char *)payload, length, TIMEOUT);
            
            if (res < 0) {
                loader_warn ("Command failed. Msg: %d; res: %d\n", index, res);
                return res;
            }
            
            if (res != length) {
                loader_warn ("Command failed. Msg: %d; res: %d; exp: %d\n", 
                    index, res, length);
                return res;
            }
        }
        
        remaining -= length;
        index++;
    }
    
    if (dump_ucode) {
        loader_msg ("Completed dumping of microcode.\n");
        close (dump_fd);
    }
    
    return 0;
}

gint
r5u87x_ucode_status (struct usb_dev_handle *handle) {
    gchar buf[1];
    gint res;
    
    res = usb_control_msg (handle, USB_RECV, 0xa4, 0, 0, buf, 1, TIMEOUT);
    if (res != 1) {
        loader_warn ("Failed to get microcode status.\n");
        return res;
    }
    
    loader_msg ("Camera reports %s microcode state.\n",
        buf[0] == 1 ? "positive" : "negative");
    
    return buf[0] == 1 ? TRUE : FALSE;
}

gint
r5u87x_ucode_version (struct usb_dev_handle *handle, gint *version) {
    gchar buf[2];
    gint res;
    
    res = usb_control_msg (handle, USB_RECV, 0xc3, 0, 0x0e, buf, 2, TIMEOUT);
    if (res != 2) {
        loader_warn ("Failed to get microcode version.\n");
        return res;
    }
    
    res = (buf[1] << 8) | buf[0];
    
    loader_msg ("Camera reports microcode version 0x%04x.\n", res);
    *version = res;
    
    return 0;
}

gint
r5u87x_ucode_enable (struct usb_dev_handle *handle) {
    gchar buf[1];
    gint res;
    buf[0] = 0;
    
    res = usb_control_msg (handle, USB_SEND, 0xa1, 0, 0, buf, 1, TIMEOUT);
    if (res != 1) {
        loader_warn ("Failed to enable microcode.\n");
        return res;
    }
    
    loader_msg ("Enabled microcode.\n");
    
    return 0;
}

gint
r5u87x_ucode_clear (struct usb_dev_handle *handle) {
    gchar buf[1];
    gint res;
    buf[0] = 1;
    res = usb_control_msg (handle, USB_SEND, 0xa6, 0, 0, buf, 1, TIMEOUT);
    
    // Windows driver liked to do a bit of zzz, so we do too.
    if (!res) {
        sleep (200);
    }
    
    if (res < 0) {
        loader_warn ("Failed to reset microcode.\n");
        return res;
    }
    
    loader_msg ("Reset microcode.\n");
    
    return res;
}

gchar*
usb_id_printf (gchar* src, struct usb_device *dev) {
    // Converts the template firmware name into one we're going to use for this
    // device.
    
    src = replace_str (src, "%pid%",
        g_strdup_printf ("%04x", dev->descriptor.idProduct));
    src = replace_str (src, "%vid%",
        g_strdup_printf ("%04x", dev->descriptor.idVendor));
    
    return src;
}

gint
load_firmware (struct usb_device *dev, usb_dev_handle *handle,
               const struct device_info *devinfo) {
    gint fd, res, dev_version;
    struct stat infobuf;

    dev_version = 0;
    
    firmware = usb_id_printf (firmware, dev);

    dump = g_strdup_printf ("r5u87x-dump-%04x-%04x.bin",
        dev->descriptor.idProduct, dev->descriptor.idVendor);
    
    loader_msg ("Found camera: %04x:%04x\n", dev->descriptor.idVendor,
        dev->descriptor.idProduct);
    
    // Open the firmware file
    if ((fd = g_open (firmware, O_RDONLY)) < 0) {
        #ifdef UCODE_PATH
        firmware = usb_id_printf (UCODE_PATH, dev);
        if ((fd = g_open (firmware, O_RDONLY)) < 0) {
            loader_error ("Failed to open %s. Does it exist?\n", firmware);
        }
        #else
        loader_error ("Failed to open %s. Does it exist?\n", firmware);
        #endif
    }
    
    // Possibly not the best way to do this, but hey, it's certainly easy
    // (without loading everything into memory, and compared to seeking around)
    if (stat (firmware, &infobuf) == -1) {
        loader_error ("Failed to get filesize of firmware (%s).\n", firmware);
    }
    
    // Check to see if there's already stuff on there.
    res = r5u87x_ucode_status (handle);
    if (res < 0) {
        return res;
    } else if (res == 1 && !force_clear) {
        // Microcode already uploaded.
        res = r5u87x_ucode_version (handle, &dev_version);
        if (dev_version == 0x0001) {
            loader_warn ("Bad version returned. You appear to be running a "
            "WDM device.\nSee http://www.bitbucket.org/ahixon/r5u87x/issue/6 "
            "for more information.\nSkipping clear for testing.\n");
        }
        
        if (res < 0) {
            return res;
        } else if (dev_version != devinfo->ucode_version &&
		   dev_version != 0x0001) {
            // Clear it out - ucode version and device version don't match.
            loader_warn ("Microcode versions don't match, clearing.\n");
            res = r5u87x_ucode_clear (handle);
            if (res < 0) {
                return res;
            }
            
            res = r5u87x_ucode_status (handle);
            if (res < 0) {
                return res;
            } else if (res == 1) {
                loader_warn ("Camera still has microcode even though we"
                    "cleared it.\n");
            }
        } else {
            // Camera already has microcode - it's all good.
            loader_msg ("Not doing anything - camera already setup.\n");
            return 0;
        }
    } else if (res == 1 && force_clear) {
        // Force reset if asked.
        res = r5u87x_ucode_clear (handle);
        if (res < 0) {
            return res;
        }
    } else if (force_clear) {
        loader_warn ("Not force-clearing because the device has no ucode.\n");
    }
    
    // Upload the microcode!
    res = r5u87x_ucode_upload (fd, handle, infobuf.st_size);
    
    // Close the microcode file
    close (fd);
    
    // and bail out if necessary.
    if (res < 0) {
        return res;
    }
    
    // Enable it after we do a successful upload.
    if (!no_load) {
        res = r5u87x_ucode_enable (handle);
        if (res < 0) {
            return res;
        }
        
        // Check versions to see if all is OK.
        dev_version = 0;
        res = r5u87x_ucode_version (handle, &dev_version);
        if (res < 0) {
            return res;
        } else if (dev_version != devinfo->ucode_version) {
            loader_warn ("Camera returned unexpected ucode version 0x%04x - "
                "expected 0x%04x\n", dev_version, devinfo->ucode_version);
            return -EBADMSG;
        }
    } else {
        loader_warn ("Skipping enabling of microcode and version checks; in "
            "pretend mode.\n");
    }
    
    return 0;
}

gint
main (gint argc, gchar *argv []) {
    GOptionContext *context;
    GError *error = NULL;
    const struct device_info *devinfo;
    struct usb_device *dev;
    usb_dev_handle *handle;
    
    g_set_prgname ("loader");
    context = g_option_context_new("- Ricoh R5U87x series firmware loader");
    g_option_context_add_main_entries (context, entries, NULL);
    
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        loader_error ("%s\n", error->message);
    }
    
    #ifdef VERSION
    loader_msg ("r5u87x firmware loader v%s\n\n", VERSION);
    #else
    loader_msg ("r5u87x firmware loader (unknown version)\n\n");
    #endif
    
    usb_init ();
    if (usb_find_busses () == 0) {
        loader_error ("Failed to find any USB busses.\n");
    } else if (usb_find_devices () == 0) {
        loader_error ("Failed to find any USB devices.\n");
    }
    
    loader_msg ("Searching for device...\n");
    
    dev = find_device (device_table, &devinfo);
    
    if (dev == NULL) {
        loader_error ("Failed to find any supported webcams.\n");
    }
    
    if (!(handle = usb_open (dev))) {
        loader_error ("Failed to open USB device.\n");
    }
    
    int res = load_firmware (dev, handle, devinfo);
    if (res < 0) {
        loader_error ("Failed to upload firmware to device: %s (code %d).\n%s",
            strerror (errno), res,
            res == -1 ? "Try running as root?\n" : "");
    } else {
        loader_msg ("\nSuccessfully uploaded firmware to device %04x:%04x!\n",
            dev->descriptor.idVendor, dev->descriptor.idProduct);
    }
    
    r5u87x_image_flip (handle, devinfo->hflip, devinfo->vflip);
    
    if (reload) {
        gint status;
        loader_msg ("Reloading uvcvideo module...\n");
        status = system ("modprobe -r uvcvideo; modprobe uvcvideo");
        if (status == -1) {
            loader_error ("Unable to create shell process.\n");
        } else if (status == 0) {
            loader_msg ("Finished.\n");
        } else {
            loader_warn ("Recevied non-zero return code: %d.\n", status);
        }
    }
    
    return 0;
}
