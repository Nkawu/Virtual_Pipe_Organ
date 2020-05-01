#include <usb_names.h>

//#define MANUFACTURER_NAME  {'F','o','o','B','a','r'}
//#define MANUFACTURER_NAME_LEN 6
#define PRODUCT_NAME    {'O','r','g','a','n','M','I','D','I'}
#define PRODUCT_NAME_LEN  9

struct usb_string_descriptor_struct usb_string_product_name = {
  2 + PRODUCT_NAME_LEN * 2,
  3,
  PRODUCT_NAME
};

//struct usb_string_descriptor_struct usb_string_manufacturer_name = {
//  2 + MANUFACTURER_NAME_LEN * 2,
//  3,
//  MANUFACTURER_NAME
//};
//
//struct usb_string_descriptor_struct usb_string_serial_number = {
//  12,
//  3,
//  {
//    0,0,7,0,0,0,7,0,0,7  }
//};
