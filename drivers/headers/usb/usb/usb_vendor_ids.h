#ifndef USB_VENDOR_IDS_H
#define USB_VENDOR_IDS_H

#include "core/stdtools.h"

// ===== USB Vendor IDs =====

// Razer Inc.
#define USB_VENDOR_RAZER                0x1532

// SteelSeries ApS
#define USB_VENDOR_STEELSERIES          0x1038


// ===== Razer Product IDs =====

// Razer Mamba Mouse variants
#define USB_PRODUCT_RAZER_MAMBA         0x0001  // Original Mamba
#define USB_PRODUCT_RAZER_MAMBA_2012    0x0024  // Mamba 2012 Edition
#define USB_PRODUCT_RAZER_MAMBA_WIRELESS 0x0044 // Mamba Wireless
#define USB_PRODUCT_RAZER_MAMBA_TE      0x0045  // Mamba Tournament Edition
#define USB_PRODUCT_RAZER_MAMBA_TE_CHROMA 0x0046 // Mamba TE Chroma
#define USB_PRODUCT_RAZER_MAMBA_ELITE   0x0073  // Mamba Elite

// Other Razer mice (for reference)
#define USB_PRODUCT_RAZER_DEATHADDER    0x0016
#define USB_PRODUCT_RAZER_NAGA          0x0015
#define USB_PRODUCT_RAZER_VIPER         0x0078
#define USB_PRODUCT_RAZER_VIPER_ULTIMATE 0x007A
#define USB_PRODUCT_RAZER_BASILISK      0x0064


// ===== SteelSeries Product IDs =====

// SteelSeries Apex Keyboards
#define USB_PRODUCT_STEELSERIES_APEX_PRO      0x1610  // Apex Pro
#define USB_PRODUCT_STEELSERIES_APEX_PRO_TKL  0x1618  // Apex Pro TKL
#define USB_PRODUCT_STEELSERIES_APEX_7        0x1612  // Apex 7
#define USB_PRODUCT_STEELSERIES_APEX_7_TKL    0x1619  // Apex 7 TKL
#define USB_PRODUCT_STEELSERIES_APEX_5        0x161C  // Apex 5
#define USB_PRODUCT_STEELSERIES_APEX_3        0x1614  // Apex 3
#define USB_PRODUCT_STEELSERIES_APEX_3_TKL    0x161E  // Apex 3 TKL

// Other SteelSeries keyboards (for reference)
#define USB_PRODUCT_STEELSERIES_APEX_M750     0x1608
#define USB_PRODUCT_STEELSERIES_APEX_M750_TKL 0x1609


// ===== Helper Functions =====

// Check if device is a Razer Mamba variant
static inline bool is_razer_mamba(uint16_t vendor_id, uint16_t product_id) {
    if (vendor_id != USB_VENDOR_RAZER) return false;
    
    return (product_id == USB_PRODUCT_RAZER_MAMBA ||
            product_id == USB_PRODUCT_RAZER_MAMBA_2012 ||
            product_id == USB_PRODUCT_RAZER_MAMBA_WIRELESS ||
            product_id == USB_PRODUCT_RAZER_MAMBA_TE ||
            product_id == USB_PRODUCT_RAZER_MAMBA_TE_CHROMA ||
            product_id == USB_PRODUCT_RAZER_MAMBA_ELITE);
}

// Check if device is a Razer mouse (any model)
static inline bool is_razer_mouse(uint16_t vendor_id, uint16_t product_id) {
    return vendor_id == USB_VENDOR_RAZER;  // All Razer mice share vendor ID
}

// Check if device is a SteelSeries Apex keyboard
static inline bool is_steelseries_apex(uint16_t vendor_id, uint16_t product_id) {
    if (vendor_id != USB_VENDOR_STEELSERIES) return false;
    
    return (product_id == USB_PRODUCT_STEELSERIES_APEX_PRO ||
            product_id == USB_PRODUCT_STEELSERIES_APEX_PRO_TKL ||
            product_id == USB_PRODUCT_STEELSERIES_APEX_7 ||
            product_id == USB_PRODUCT_STEELSERIES_APEX_7_TKL ||
            product_id == USB_PRODUCT_STEELSERIES_APEX_5 ||
            product_id == USB_PRODUCT_STEELSERIES_APEX_3 ||
            product_id == USB_PRODUCT_STEELSERIES_APEX_3_TKL);
}

// Check if device is a SteelSeries keyboard (any model)
static inline bool is_steelseries_keyboard(uint16_t vendor_id, uint16_t product_id) {
    return vendor_id == USB_VENDOR_STEELSERIES;  // All SteelSeries keyboards share vendor ID
}

#endif // USB_VENDOR_IDS_H
