/* Copyright (c) 2014 John Schember <john@nachtimwald.com>
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// macOS users, swap/adjust these includes:
// https://github.com/user-none/sixaxispairer/issues/5
// pkg-config --cflags hidapi
// #include </opt/homebrew/Cellar/hidapi/0.13.1/include/hidapi/hidapi.h>
#include <hidapi/hidapi.h>

#define VENDOR     0x054c
#define PRODUCT_V1 0x05c4
#define PRODUCT_V2 0x09cc
/* 0xf5   == (0x03f5 & ~(3 << 8))
 * 0x03f5 == (0xf5 | (3 << 8))
 * HIDAPI will automatically add (3 << 8 to the report id.
 * Other tools for setting the report id use hid libraries which
 * don't automatically do this. */
// https://coral.googlesource.com/bluez-imx/+/refs/tags/5.50/plugins/sixaxis.c#175
#define GET_MAC_REPORT_ID 0x12
// https://eleccelerator.com/unpairing-a-dualshock-4-and-setting-a-new-bdaddr/
// https://coral.googlesource.com/bluez-imx/+/refs/tags/5.50/plugins/sixaxis.c#224
#define SET_MAC_REPORT_ID 0x13

static unsigned char char_to_nible(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return 255;
}

/* return 1 on success, 0 on failure. */
static int mac_to_bytes(const char *in, size_t in_len, unsigned char *out, size_t out_len)
{
    const char *p;
    size_t      i=0;

    if (in == NULL || out == NULL || in_len == 0 || out_len == 0)
        return 0;

    memset(out, 0, out_len);
    p = in;
    char temp[6];
    while (p+1 < in+in_len && i < out_len) {
        if (*p == ':') {
            p++;
            continue;
        }

        if (!isxdigit(*p) || !isxdigit(*(p+1))) {
            return 0;
        }

        temp[i] = (char_to_nible(*p) << 4) | (char_to_nible(*(p+1) & 0xff));
        i++;
        p += 2;
    }

    // FIXME: This is jank, but it's late. DS4 is little-endian
    out[0] = temp[5];
    out[1] = temp[4];
    out[2] = temp[3];
    out[3] = temp[2];
    out[4] = temp[1];
    out[5] = temp[0];

    if (p < in+in_len)
        return 0;
    return 1;
}

static void pair_device(hid_device *dev, const char *mac, size_t mac_len)
{
    unsigned char buf[23];
    int           ret;

    memset(buf, 0, sizeof(buf));
    buf[0] = SET_MAC_REPORT_ID;
    buf[1] = 0x0;
    if ((mac_len != 12 && mac_len != 17) || !mac_to_bytes(mac, mac_len, buf+1, sizeof(buf)-2)) {
        printf("Invalid mac\n");
        return;
    }
    // https://coral.googlesource.com/bluez-imx/+/refs/tags/5.50/plugins/sixaxis.c#224
    memset(buf + 7, 0, 16);
    // An actual key is set in https://eleccelerator.com/unpairing-a-dualshock-4-and-setting-a-new-bdaddr/, but that didn't seem to work for me.

    ret = hid_send_feature_report(dev, buf, sizeof(buf));
    if (ret == -1) {
        printf("Failed to set mac\n");
    }
}

static void show_pairing(hid_device *dev)
{
    unsigned char buf[16];
    int           ret;

    memset(buf, 0, sizeof(buf));
    buf[0] = GET_MAC_REPORT_ID;
    buf[1] = 0x0;

    ret = hid_get_feature_report(dev, buf, sizeof(buf));
    if (ret < 7) {
        printf("Read error\n");
        return;
    }
    printf("%02x:%02x:%02x:%02x:%02x:%02x\n", buf[15], buf[14], buf[13], buf[12], buf[11], buf[10]);
}

int main(int argc, char **argv)
{
    hid_device *dev;

    if ((argc != 1 && argc != 2) ||
        (argc == 2 && (strncmp(argv[1], "-h", 2) == 0 || strncmp(argv[1], "--help", 6) == 0)))
    {
        printf("usage:\n\t%s [mac]\n", argv[0]);
        return 0;
    }


    dev = hid_open(VENDOR, PRODUCT_V1, NULL);
    if (dev == NULL) {
        dev = hid_open(VENDOR, PRODUCT_V2, NULL);
    }

    if (dev == NULL) {
        fprintf(stderr, "Could not find DualShock 4 controller\n");
        return 1;
    }

    if (argc == 2) {
        pair_device(dev, argv[1], strlen(argv[1]));
    } else {
        show_pairing(dev);
    }

    hid_close(dev);
    return 0;
}
