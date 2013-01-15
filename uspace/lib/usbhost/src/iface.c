/*
 * Copyright (c) 2011 Jan Vesely
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup libusbhost
 * @{
 */
/** @file
 * @brief HCD DDF interface implementation
 */

#include <ddf/driver.h>
#include <errno.h>

#include <usb/debug.h>
#include <usb/host/endpoint.h>
#include <usb/host/hcd.h>
#include "ddf_helpers.h"

/** Find device handle by address interface function.
 *
 * @param[in] fun DDF function that was called.
 * @param[in] address Address in question.
 * @param[out] handle Where to store device handle if found.
 * @return Error code.
 */
static int find_by_address(ddf_fun_t *fun, usb_address_t address,
    devman_handle_t *handle)
{
	assert(fun);
	hcd_t *hcd = dev_to_hcd(ddf_fun_get_dev(fun));
	assert(hcd);
	return usb_device_manager_get_info_by_address(
	    &hcd->dev_manager, address, handle, NULL);
}

/** Register endpoint interface function.
 * @param fun DDF function.
 * @param address USB address of the device.
 * @param endpoint USB endpoint number to be registered.
 * @param transfer_type Endpoint's transfer type.
 * @param direction USB communication direction the endpoint is capable of.
 * @param max_packet_size Maximu size of packets the endpoint accepts.
 * @param interval Preferred timeout between communication.
 * @return Error code.
 */
static int register_endpoint(
    ddf_fun_t *fun, usb_address_t address, usb_endpoint_t endpoint,
    usb_transfer_type_t transfer_type, usb_direction_t direction,
    size_t max_packet_size, unsigned interval)
{
	assert(fun);
	hcd_t *hcd = dev_to_hcd(ddf_fun_get_dev(fun));
	assert(hcd);
	const size_t size = max_packet_size;
	const usb_target_t target = {{.address = address, .endpoint = endpoint}};

	usb_log_debug("Register endpoint %d:%d %s-%s %zuB %ums.\n",
	    address, endpoint, usb_str_transfer_type(transfer_type),
	    usb_str_direction(direction), max_packet_size, interval);

	return hcd_add_ep(hcd, target, direction, transfer_type,
	    max_packet_size, size);
}

/** Unregister endpoint interface function.
 * @param fun DDF function.
 * @param address USB address of the endpoint.
 * @param endpoint USB endpoint number.
 * @param direction Communication direction of the enpdoint to unregister.
 * @return Error code.
 */
static int unregister_endpoint(
    ddf_fun_t *fun, usb_address_t address,
    usb_endpoint_t endpoint, usb_direction_t direction)
{
	assert(fun);
	hcd_t *hcd = dev_to_hcd(ddf_fun_get_dev(fun));
	assert(hcd);
	const usb_target_t target = {{.address = address, .endpoint = endpoint}};
	usb_log_debug("Unregister endpoint %d:%d %s.\n",
	    address, endpoint, usb_str_direction(direction));
	return hcd_remove_ep(hcd, target, direction);
}

/** Inbound communication interface function.
 * @param fun DDF function.
 * @param target Communication target.
 * @param setup_data Data to use in setup stage (control transfers).
 * @param data Pointer to data buffer.
 * @param size Size of the data buffer.
 * @param callback Function to call on communication end.
 * @param arg Argument passed to the callback function.
 * @return Error code.
 */
static int usb_read(ddf_fun_t *fun, usb_target_t target, uint64_t setup_data,
    uint8_t *data, size_t size, usbhc_iface_transfer_in_callback_t callback,
    void *arg)
{
	return hcd_send_batch(dev_to_hcd(ddf_fun_get_dev(fun)), target,
	    USB_DIRECTION_IN, data, size, setup_data, callback, NULL, arg,
	    "READ");
}

/** Outbound communication interface function.
 * @param fun DDF function.
 * @param target Communication target.
 * @param setup_data Data to use in setup stage (control transfers).
 * @param data Pointer to data buffer.
 * @param size Size of the data buffer.
 * @param callback Function to call on communication end.
 * @param arg Argument passed to the callback function.
 * @return Error code.
 */
static int usb_write(ddf_fun_t *fun, usb_target_t target, uint64_t setup_data,
    const uint8_t *data, size_t size,
    usbhc_iface_transfer_out_callback_t callback, void *arg)
{
	return hcd_send_batch(dev_to_hcd(ddf_fun_get_dev(fun)),
	    target, USB_DIRECTION_OUT, (uint8_t*)data, size, setup_data, NULL,
	    callback, arg, "WRITE");
}

/** usbhc Interface implementation using hcd_t from libusbhost library. */
usbhc_iface_t hcd_iface = {
	.get_handle = find_by_address,

	.register_endpoint = register_endpoint,
	.unregister_endpoint = unregister_endpoint,

	.read = usb_read,
	.write = usb_write,
};

/**
 * @}
 */
