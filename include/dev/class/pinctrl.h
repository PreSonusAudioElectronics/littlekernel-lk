/*!
 * \file pinctrl.h
 * \author D. Anderson
 * \brief This pin controller just exists to facilitate access to the 
 * gpio driver.  It was only created because it was proving exceedling
 * awkward to directly access gpio pins.
 * 
 * Copyright (c) 2022 Presonus Audio Electronics
 * 
 */

#ifndef _dev_class_pinctrl_h
#define _dev_class_pinctrl_h


#include <dev/driver.h>

__BEGIN_CDECLS

struct pin_controller_ops
{
    struct driver_ops std;
    status_t (*pin_write)(struct device *dev, uint8_t pin_idx, uint8_t wr_val);
};

struct device *pin_controller_get_device_by_id (int id);

__END_CDECLS

#endif // _dev_class_pinctrl_h
