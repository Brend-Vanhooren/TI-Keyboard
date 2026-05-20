/*
 *--------------------------------------
 * Program Name: TI-Keyboard
 * Author: Brend Vanhooren
 * Description: Use ti-84 as keyboard via usb.
 *--------------------------------------
*/

#include "defines.h"

#include <keypadc.h>
#include <usbdrvce.h>
#include <graphx.h>
#include <string.h>
#include <stdint.h>

int height_offset = 8;

uint8_t prev_report[8] = {0};

void PrintCentered(const char *str);
void PrintReport(uint8_t *report);

static const uint8_t hid_report_descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)

    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0xE0,        //   Usage Minimum (224) - Left Control
    0x29, 0xE7,        //   Usage Maximum (231) - Right GUI
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute) - Modifier bits

    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Constant) - Reserved byte

    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data, Array) - Keycodes

    0xC0               // End Collection
};


static const usb_string_descriptor_t product_name = {
    .bLength = 2+2*11, // 2 + 2*(length of .bStreng)
    .bDescriptorType = USB_STRING_DESCRIPTOR,
    .bString = L"TI-Keyboard",
};

static const usb_string_descriptor_t manufacturer = {
    .bLength = 2+2*15,
    .bDescriptorType = USB_STRING_DESCRIPTOR,
    .bString = L"Brend Vanhooren",
};

static usb_string_descriptor_t serial = {
    .bLength = 2+2*4,
    .bDescriptorType = USB_STRING_DESCRIPTOR,
    .bString = L"0001",
};

static const usb_string_descriptor_t *strings[] = { &manufacturer, &product_name,  &serial};

static const usb_string_descriptor_t langids = {
    .bLength = 4,
    .bDescriptorType = USB_STRING_DESCRIPTOR,
    .bString = {DEFAULT_LANGID},
};

static struct {
    usb_configuration_descriptor_t configuration;
    struct {
    usb_interface_descriptor_t     interface;
    usb_hid_descriptor_t           hid;
    usb_endpoint_descriptor_t      endpoint_in;
    } interface0;
} configuration1 = {
    .configuration = {
        .bLength = sizeof(configuration1.configuration),
        .bDescriptorType = USB_CONFIGURATION_DESCRIPTOR,
        .wTotalLength = sizeof(configuration1),
        .bNumInterfaces = 1, // number of interfaces
        .bConfigurationValue = 1, // set configuration #1
        .iConfiguration = 0,
        .bmAttributes = USB_BUS_POWERED | USB_NO_REMOTE_WAKEUP,
        .bMaxPower = 50, // 50 x 2 mA
    },
    .interface0 = {
        .interface = {
            .bLength = sizeof(configuration1.interface0.interface),
            .bDescriptorType = USB_INTERFACE_DESCRIPTOR,
            .bInterfaceNumber = 0, // only 1 interface
            .bAlternateSetting = 0, // only 1 mode
            .bNumEndpoints = 1, // only 1 endpoint
            .bInterfaceClass = USB_HID_CLASS, // Human Interface Device
            .bInterfaceSubClass = 0x01, // boot interface
            .bInterfaceProtocol = 0x01, // keyboard
            .iInterface = 0, // optional string index (e.g. "HID Keyboard Interface")
        },
        .hid = {
            .bLength = sizeof(configuration1.interface0.hid),
            .bDescriptorType = USB_HID_DESCRIPTOR,
            .bcdHID = 0x0111, // HID version 1.11
            .bCountryCode = 0x02, // Belgium
            .bNumDescriptors = 1, // only 1 report descriptor
            .bReportDescriptorType = USB_HID_REPORT_DESCRIPTOR,
            .wDescriptorLength = sizeof(hid_report_descriptor),
        },
        .endpoint_in = {
            .bLength = sizeof(configuration1.interface0.endpoint_in),
            .bDescriptorType = USB_ENDPOINT_DESCRIPTOR,
            .bEndpointAddress = USB_DEVICE_TO_HOST | 1,
            .bmAttributes = USB_INTERRUPT_TRANSFER,
            .wMaxPacketSize = 8, // standerd for HID keyboards
            .bInterval = 10,
        },
    },
};

static const usb_configuration_descriptor_t *configurations[] = { &configuration1.configuration };

static const usb_device_descriptor_t device = {
    .bLength = sizeof(device),
    .bDescriptorType = USB_DEVICE_DESCRIPTOR,
    .bcdUSB = 0x0110, //USB version 1.1
    .bDeviceClass = 0, // each interface defines its own class
    .bDeviceSubClass = 0, // ignored 
    .bDeviceProtocol = 0, // ignored
    .bMaxPacketSize0 = 64,
    .idVendor = 0x0451, // Texas Instruments
    .idProduct = 0x1D50, // OpenMoko (community VID)
    .bcdDevice = 0x606F, // arbitrary unused PID
    .iManufacturer = 1, // referce to place in strings
    .iProduct = 2, // referce to place in strings
    .iSerialNumber = 3, // referce to place in strings
    .bNumConfigurations = 1,
};


static const usb_standard_descriptors_t standard_descriptors = {
    .device = &device,
    .configurations = configurations,
    .langids = &langids,
    .numStrings = sizeof(strings) / sizeof(*strings),
    .strings = strings,
};

static usb_error_t usb_event_handler(usb_event_t event, void *eventData, usb_callback_data_t *callbackData) {
    (void)callbackData;
    usb_error_t error = USB_SUCCESS;
    usb_device_t connected_device = usb_FindDevice(NULL, NULL, USB_SKIP_HUBS);
    usb_endpoint_t endpoint = usb_GetDeviceEndpoint(connected_device, 0);


    if (event == USB_DEFAULT_SETUP_EVENT) {
        usb_control_setup_t *setup = (usb_control_setup_t *)eventData;
        if (setup->bRequest == USB_GET_DESCRIPTOR_REQUEST && // host is requesting descriptiors
            setup->wValue == 0x0200 && // host is requesting configuration descripor #0
            setup->wLength == sizeof(configuration1)) { // check amount of bytes

            usb_Transfer(endpoint, &configuration1, sizeof(configuration1), 5, NULL);
            error = USB_IGNORE;
            PrintCentered("Configuration sent");
        }
        else if (setup->bRequest == USB_GET_DESCRIPTOR_REQUEST &&
                 setup->wValue == 0x2200){ // HID report desciptor type

                usb_Transfer(endpoint, (void*)hid_report_descriptor, sizeof(hid_report_descriptor), 5, NULL);
                error = USB_IGNORE;
                PrintCentered("HID report sent");
                }
    else if (event == USB_DEVICE_CONNECTED_EVENT) {
        PrintCentered("USB plugged in");
    }
    else if (event == USB_DEVICE_DISCONNECTED_EVENT) {
        PrintCentered("USB unplugged");
    }
    }
    return error;
}


int main(void) {
    gfx_Begin();

    gfx_FillScreen(0);
    gfx_SetTextFGColor(255);
    gfx_SetTextBGColor(0);
    gfx_SetTextTransparentColor(0);

    usb_error_t error;
    if ((error = usb_Init(usb_event_handler, NULL, &standard_descriptors, USB_DEFAULT_INIT_FLAGS)) == USB_SUCCESS) {
        PrintCentered("USB Initialised");
    }

    bool running = true;
    usb_device_t connected_device = NULL;
    usb_endpoint_t endpoint_in = NULL;

    bool second = false;
    bool alfa = false;

    while(running){
        kb_Scan();
        usb_HandleEvents();

        if (connected_device == NULL) {
            connected_device = usb_FindDevice(NULL, NULL, USB_SKIP_HUBS);
            if (connected_device != NULL) PrintCentered("Device found");
        }
        if (endpoint_in == NULL) {
            endpoint_in = usb_GetDeviceEndpoint(connected_device, USB_DEVICE_TO_HOST | 1);
            if (endpoint_in != NULL) PrintCentered("Endpoint_in found");
        }


        if (kb_Data[6] & kb_Clear) break; // quit program when pressing clear
        if (kb_Data[1] & kb_2nd) second = !second;
        if (kb_Data[2] & kb_Alpha) alfa = !alfa;

        // Make a report to sent to the host
        uint8_t report[8] = {0};
        uint8_t report_index = 2;
        bool shortcut_pressed = false;



        for (int row = 1; row <= 7; row++) {
            for (int col = 0; col <= 7; col++) {
                if (kb_Data[row] & (1 << col)) {
                    if (find_shortcut(row, col, report)) {
                        shortcut_pressed = true;
                        break;
                    };

                    uint8_t hid_code = find_hid_code(row, col, second, alfa);
                    if (hid_code == 0) continue;
                    report[report_index] = hid_code;
                    report_index ++;
                    second = false; alfa = false;
                    if (report_index >= 8) break; // Stop making report if there is no more room
                }
            }
            if (report_index >= 8 || shortcut_pressed) break; // Stop making report if there is no more room, or a shortcut is pressed
        }
            
        // Send report if new keys have been pressed/released
        if (memcmp(report, prev_report, sizeof(report)) != 0) {
            PrintReport(report);

            send_report(report, sizeof(report), endpoint_in);
            memcpy(prev_report, report, sizeof(report));
            }
    }
    usb_Cleanup();
    gfx_End();
    return 0;
}


void PrintCentered(const char *str) {
    if (height_offset >= GFX_LCD_HEIGHT) { // Clear screen if there is no more room
        gfx_FillScreen(0);
        height_offset = 8;
    }
    gfx_PrintStringXY(str, (GFX_LCD_WIDTH - gfx_GetStringWidth(str)) / 2, height_offset);
    height_offset += 8;
}

void PrintReport(uint8_t *report) {
    int x = (GFX_LCD_WIDTH - (9 * 3 * 8)) / 2; // rough centering for 9 fields

    gfx_PrintStringXY("Report:", x, height_offset);
    gfx_SetTextXY(x + 8 * 8, height_offset); // small indent

    for (int i = 0; i < 8; i++) {
        gfx_PrintByteHex(report[i]);  // prints 2-digit uppercase hex
        gfx_PrintChar(' ');
    }

    height_offset += 8;
    if (height_offset >= GFX_LCD_HEIGHT - 8) {
        gfx_FillScreen(0);
        height_offset = 0;
    }
}
