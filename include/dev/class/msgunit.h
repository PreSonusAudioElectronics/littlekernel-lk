/*
 * Copyright 2019 NXP
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __DEV_CLASS_MSGUNIT_H
#define __DEV_CLASS_MSGUNIT_H

#include <compiler.h>
#include <dev/driver.h>

/*
    Usage concept of this driver is to have one driver instance bound to one
    Messaging Unit Channel.  The channel binding is defined in the device tree
    along with the peripheral base address.
*/

__BEGIN_CDECLS

typedef void(*msgunit_tx_cb_t)(void);
typedef void(*msgunit_rx_cb_t)(uint32_t);

struct msgunit_ops {
    struct driver_ops std;
    status_t (*send_msg)(struct device *dev, uint32_t msg);
    status_t (*receive_msg)(struct device *dev, uint32_t*dst);
    status_t (*register_tx_callback)(struct device *dev, msgunit_tx_cb_t cb);
    status_t (*register_rx_callback)(struct device *dev, msgunit_rx_cb_t cb);
    status_t (*start)(struct device *dev);
    status_t (*stop)(struct device *dev);
};

status_t class_msgunit_register_tx_callback(struct device *dev, msgunit_tx_cb_t cb);
status_t class_msgunit_register_rx_callback(struct device *dev, msgunit_rx_cb_t cb);
status_t class_msgunit_start(struct device *dev);
status_t class_msgunit_stop(struct device *dev);
status_t class_msgunit_send_msg(struct device *dev, uint32_t msg);
struct device * class_msgunit_get_device_by_id(int bus_id);


__END_CDECLS

#endif /* __DEV_CLASS_MSGUNIT_H */
