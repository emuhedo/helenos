/*
 * Copyright (c) 2014 Jiri Svoboda
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

/** @addtogroup hdaudio
 * @{
 */
/** @file High Definition Audio driver
 */

#include <assert.h>
#include <bitops.h>
#include <ddi.h>
#include <device/hw_res_parsed.h>
#include <stdio.h>
#include <errno.h>
#include <str_error.h>
#include <ddf/driver.h>
#include <ddf/interrupt.h>
#include <ddf/log.h>

#include "hdactl.h"
#include "hdaudio.h"
#include "spec/regs.h"

#define NAME "hdaudio"

static int hda_dev_add(ddf_dev_t *dev);
static int hda_dev_remove(ddf_dev_t *dev);
static int hda_dev_gone(ddf_dev_t *dev);
static int hda_fun_online(ddf_fun_t *fun);
static int hda_fun_offline(ddf_fun_t *fun);

static void hdaudio_interrupt(ddf_dev_t *, ipc_callid_t, ipc_call_t *);

static driver_ops_t driver_ops = {
	.dev_add = &hda_dev_add,
	.dev_remove = &hda_dev_remove,
	.dev_gone = &hda_dev_gone,
	.fun_online = &hda_fun_online,
	.fun_offline = &hda_fun_offline
};

static driver_t hda_driver = {
	.name = NAME,
	.driver_ops = &driver_ops
};

irq_pio_range_t hdaudio_irq_pio_ranges[] = {
	{
		.base = 0,
		.size = 8192
	}
};

irq_cmd_t hdaudio_irq_commands[] = {
	{
		.cmd = CMD_PIO_READ_8,
		.addr = NULL,
		.dstarg = 2
	},
	{
		.cmd = CMD_AND,
		.value = BIT_V(uint8_t, rirbsts_intfl),
		.srcarg = 2,
		.dstarg = 3
	},
	{
		.cmd = CMD_PREDICATE,
		.value = 2,
		.srcarg = 3
	},
	{
		.cmd = CMD_PIO_WRITE_8,
		.addr = NULL,
		.value = BIT_V(uint8_t, rirbsts_intfl),
	},
	{
		.cmd = CMD_ACCEPT
	}
};

irq_code_t hdaudio_irq_code = {
	.rangecount = sizeof(hdaudio_irq_pio_ranges) / sizeof(irq_pio_range_t),
	.ranges = hdaudio_irq_pio_ranges,
	.cmdcount = sizeof(hdaudio_irq_commands) / sizeof(irq_cmd_t),
	.cmds = hdaudio_irq_commands
};

static int hda_dev_add(ddf_dev_t *dev)
{
	ddf_fun_t *fun_a;
	hda_t *hda = NULL;
	hw_res_list_parsed_t res;
	void *regs;
	int rc;

	ddf_msg(LVL_NOTE, "hda_dev_add()");

	hda = ddf_dev_data_alloc(dev, sizeof(hda_t));
	if (hda == NULL) {
		ddf_msg(LVL_ERROR, "Failed allocating soft state.\n");
		rc = ENOMEM;
		goto error;
	}

	ddf_msg(LVL_NOTE, "create parent sess");
	hda->parent_sess = ddf_dev_parent_sess_create(dev,
	    EXCHANGE_SERIALIZE);
	if (hda->parent_sess == NULL) {
		ddf_msg(LVL_ERROR, "Failed connecting parent driver.\n");
		rc = ENOMEM;
		goto error;
	}

	ddf_msg(LVL_NOTE, "get HW res list");
	hw_res_list_parsed_init(&res);
	rc = hw_res_get_list_parsed(hda->parent_sess, &res, 0);
	if (rc != EOK) {
		ddf_msg(LVL_ERROR, "Failed getting resource list.\n");
		goto error;
	}

	if (res.mem_ranges.count != 1) {
		ddf_msg(LVL_ERROR, "Expected exactly one memory range.\n");
		rc = EINVAL;
		goto error;
	}

	hda->rwbase = RNGABS(res.mem_ranges.ranges[0]);
	hda->rwsize = RNGSZ(res.mem_ranges.ranges[0]);

	ddf_msg(LVL_NOTE, "hda reg base: %" PRIx64,
	     RNGABS(res.mem_ranges.ranges[0]));

	if (hda->rwsize < sizeof(hda_regs_t)) {
		ddf_msg(LVL_ERROR, "Memory range is too small.");
		rc = EINVAL;
		goto error;
	}

	ddf_msg(LVL_NOTE, "enable PIO");
	rc = pio_enable((void *)(uintptr_t)hda->rwbase, hda->rwsize, &regs);
	if (rc != EOK) {
		ddf_msg(LVL_ERROR, "Error enabling PIO range.");
		goto error;
	}

	hda->regs = (hda_regs_t *)regs;

	ddf_msg(LVL_NOTE, "IRQs: %d", res.irqs.count);
	if (res.irqs.count != 1) {
		ddf_msg(LVL_ERROR, "Unexpected IRQ count %d (!= 1)",
		    res.irqs.count);
		goto error;
	}
	ddf_msg(LVL_NOTE, "interrupt no: %d", res.irqs.irqs[0]);

	hda_regs_t *rphys = (hda_regs_t *)(uintptr_t)hda->rwbase;
	hdaudio_irq_pio_ranges[0].base = (uintptr_t)hda->rwbase;
	hdaudio_irq_commands[0].addr = (void *)&rphys->rirbsts;
	hdaudio_irq_commands[3].addr = (void *)&rphys->rirbsts;
	ddf_msg(LVL_NOTE, "range0.base=%x", hdaudio_irq_pio_ranges[0].base);
	ddf_msg(LVL_NOTE, "cmd0.addr=%p", hdaudio_irq_commands[0].addr);
	ddf_msg(LVL_NOTE, "cmd3.addr=%p", hdaudio_irq_commands[3].addr);

	rc = register_interrupt_handler(dev, res.irqs.irqs[0],
	    hdaudio_interrupt, &hdaudio_irq_code);
	if (rc != EOK) {
		ddf_msg(LVL_ERROR, "Failed registering interrupt handler. (%d)",
		    rc);
		goto error;
	}

	if (hda_ctl_init(hda) == NULL) {
		rc = EIO;
		goto error;
	}

	ddf_msg(LVL_NOTE, "create function");
	fun_a = ddf_fun_create(dev, fun_exposed, "a");
	if (fun_a == NULL) {
		ddf_msg(LVL_ERROR, "Failed creating function 'a'.");
		rc = ENOMEM;
		goto error;
	}

	hda->fun_a = fun_a;

	rc = ddf_fun_bind(fun_a);
	if (rc != EOK) {
		ddf_msg(LVL_ERROR, "Failed binding function 'a'.");
		ddf_fun_destroy(fun_a);
		goto error;
	}

	ddf_fun_add_to_category(fun_a, "virtual");
	return EOK;
error:
	if (hda != NULL) {
		if (hda->ctl != NULL)
			hda_ctl_fini(hda->ctl);
	}

	ddf_msg(LVL_NOTE, "Failing hda_dev_add() -> %d", rc);
	return rc;
}

static int hda_dev_remove(ddf_dev_t *dev)
{
	hda_t *hda = (hda_t *)ddf_dev_data_get(dev);
	int rc;

	ddf_msg(LVL_DEBUG, "hda_dev_remove(%p)", dev);

	if (hda->fun_a != NULL) {
		rc = ddf_fun_offline(hda->fun_a);
		if (rc != EOK)
			return rc;

		rc = ddf_fun_unbind(hda->fun_a);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

static int hda_dev_gone(ddf_dev_t *dev)
{
	hda_t *hda = (hda_t *)ddf_dev_data_get(dev);
	int rc;

	ddf_msg(LVL_DEBUG, "hda_dev_remove(%p)", dev);

	if (hda->fun_a != NULL) {
		rc = ddf_fun_unbind(hda->fun_a);
		if (rc != EOK)
			return rc;
	}

	return EOK;
}

static int hda_fun_online(ddf_fun_t *fun)
{
	ddf_msg(LVL_DEBUG, "hda_fun_online()");
	return ddf_fun_online(fun);
}

static int hda_fun_offline(ddf_fun_t *fun)
{
	ddf_msg(LVL_DEBUG, "hda_fun_offline()");
	return ddf_fun_offline(fun);
}

static void hdaudio_interrupt(ddf_dev_t *dev, ipc_callid_t iid,
    ipc_call_t *icall)
{
	hda_t *hda = (hda_t *)ddf_dev_data_get(dev);

	ddf_msg(LVL_NOTE, "## interrupt ##");
	hda_ctl_interrupt(hda->ctl);
}

int main(int argc, char *argv[])
{
	printf(NAME ": High Definition Audio driver\n");
	ddf_log_init(NAME);
	return ddf_driver_main(&hda_driver);
}

/** @}
 */
