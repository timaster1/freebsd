/*-
 * Copyright (c) 2005 Poul-Henning Kamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_hpet.h>

#define HPET_VENDID_AMD		0x4353
#define HPET_VENDID_INTEL	0x8086

ACPI_SERIAL_DECL(hpet, "ACPI HPET support");

static devclass_t acpi_hpet_devclass;

/* ACPI CA debugging */
#define _COMPONENT	ACPI_TIMER
ACPI_MODULE_NAME("HPET")

struct acpi_hpet_softc {
	device_t		dev;
	struct resource		*mem_res;
	ACPI_HANDLE		handle;
};

static u_int hpet_get_timecount(struct timecounter *tc);
static void acpi_hpet_test(struct acpi_hpet_softc *sc);

static char *hpet_ids[] = { "PNP0103", NULL };

struct timecounter hpet_timecounter = {
	.tc_get_timecount =	hpet_get_timecount,
	.tc_counter_mask =	~0u,
	.tc_name =		"HPET",
	.tc_quality =		900,
};

static u_int
hpet_get_timecount(struct timecounter *tc)
{
	struct acpi_hpet_softc *sc;

	sc = tc->tc_priv;
	return (bus_read_4(sc->mem_res, HPET_MAIN_COUNTER));
}

static void
hpet_enable(struct acpi_hpet_softc *sc)
{
	uint32_t val;

	val = bus_read_4(sc->mem_res, HPET_CONFIG);
	val &= ~HPET_CNF_LEG_RT;
	val |= HPET_CNF_ENABLE;
	bus_write_4(sc->mem_res, HPET_CONFIG, val);
}

static void
hpet_disable(struct acpi_hpet_softc *sc)
{
	uint32_t val;

	val = bus_read_4(sc->mem_res, HPET_CONFIG);
	val &= ~HPET_CNF_ENABLE;
	bus_write_4(sc->mem_res, HPET_CONFIG, val);
}

static ACPI_STATUS
acpi_hpet_find(ACPI_HANDLE handle, UINT32 level, void *context,
    void **status)
{
	char 		**ids;
	uint32_t	id = (uint32_t)(uintptr_t)context;
	uint32_t	uid = 0;

	for (ids = hpet_ids; *ids != NULL; ids++) {
		if (acpi_MatchHid(handle, *ids))
		        break;
	}
	if (*ids == NULL)
		return (AE_OK);
	if (ACPI_FAILURE(acpi_GetInteger(handle, "_UID", &uid)))
		uid = 0;
	if (id == uid)
		*((int *)status) = 1;
	return (AE_OK);
}

/* Discover the HPET via the ACPI table of the same name. */
static void 
acpi_hpet_identify(driver_t *driver, device_t parent)
{
	ACPI_TABLE_HPET *hpet;
	ACPI_STATUS	status;
	device_t	child;
	int 		i, found;

	/* Only one HPET device can be added. */
	if (devclass_get_device(acpi_hpet_devclass, 0))
		return;
	for (i = 1; ; i++) {
		/* Search for HPET table. */
		status = AcpiGetTable(ACPI_SIG_HPET, i, (ACPI_TABLE_HEADER **)&hpet);
		if (ACPI_FAILURE(status))
			return;
		/* Search for HPET device with same ID. */
		found = 0;
		AcpiWalkNamespace(ACPI_TYPE_DEVICE, ACPI_ROOT_OBJECT,
		    100, acpi_hpet_find, NULL, (void *)(uintptr_t)hpet->Sequence, (void *)&found);
		/* If found - let it be probed in normal way. */
		if (found)
			continue;
		/* If not - create it from table info. */
		child = BUS_ADD_CHILD(parent, ACPI_DEV_BASE_ORDER, "acpi_hpet", 0);
		if (child == NULL) {
			printf("%s: can't add child\n", __func__);
			continue;
		}
		bus_set_resource(child, SYS_RES_MEMORY, 0, hpet->Address.Address,
		    HPET_MEM_WIDTH);
	}
}

static int
acpi_hpet_probe(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	if (acpi_disabled("hpet"))
		return (ENXIO);
	if (acpi_get_handle(dev) != NULL &&
	    ACPI_ID_PROBE(device_get_parent(dev), dev, hpet_ids) == NULL)
		return (ENXIO);

	device_set_desc(dev, "High Precision Event Timer");
	return (0);
}

static int
acpi_hpet_attach(device_t dev)
{
	struct acpi_hpet_softc *sc;
	int rid, num_timers;
	uint32_t val, val2;
	uintmax_t freq;
	uint16_t vendor;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->handle = acpi_get_handle(dev);

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		return (ENOMEM);

	/* Validate that we can access the whole region. */
	if (rman_get_size(sc->mem_res) < HPET_MEM_WIDTH) {
		device_printf(dev, "memory region width %ld too small\n",
		    rman_get_size(sc->mem_res));
		bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
		return (ENXIO);
	}

	/* Be sure timer is enabled. */
	hpet_enable(sc);

	/* Read basic statistics about the timer. */
	val = bus_read_4(sc->mem_res, HPET_PERIOD);
	if (val == 0) {
		device_printf(dev, "invalid period\n");
		hpet_disable(sc);
		bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
		return (ENXIO);
	}

	freq = (1000000000000000LL + val / 2) / val;
	if (bootverbose) {
		val = bus_read_4(sc->mem_res, HPET_CAPABILITIES);

		/*
		 * ATI/AMD violates IA-PC HPET (High Precision Event Timers)
		 * Specification and provides an off by one number
		 * of timers/comparators.
		 * Additionally, they use unregistered value in VENDOR_ID field.
		 */
		num_timers = 1 + ((val & HPET_CAP_NUM_TIM) >> 8);
		vendor = val >> 16;
		if (vendor == HPET_VENDID_AMD && num_timers > 0)
			num_timers--;
		device_printf(dev,
		    "vend: 0x%x rev: 0x%x num: %d hz: %jd opts:%s%s\n",
		    vendor, val & HPET_CAP_REV_ID,
		    num_timers, freq,
		    (val & HPET_CAP_LEG_RT) ? " legacy_route" : "",
		    (val & HPET_CAP_COUNT_SIZE) ? " 64-bit" : "");
	}

	if (testenv("debug.acpi.hpet_test"))
		acpi_hpet_test(sc);

	/*
	 * Don't attach if the timer never increments.  Since the spec
	 * requires it to be at least 10 MHz, it has to change in 1 us.
	 */
	val = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	DELAY(1);
	val2 = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	if (val == val2) {
		device_printf(dev, "HPET never increments, disabling\n");
		hpet_disable(sc);
		bus_free_resource(dev, SYS_RES_MEMORY, sc->mem_res);
		return (ENXIO);
	}
	/* Announce first HPET as timecounter. */
	if (device_get_unit(dev) == 0) {
		hpet_timecounter.tc_frequency = freq;
		hpet_timecounter.tc_priv = sc;
		tc_init(&hpet_timecounter);
	}
	return (0);
}

static int
acpi_hpet_detach(device_t dev)
{
	ACPI_FUNCTION_TRACE((char *)(uintptr_t) __func__);

	/* XXX Without a tc_remove() function, we can't detach. */
	return (EBUSY);
}

static int
acpi_hpet_suspend(device_t dev)
{
	struct acpi_hpet_softc *sc;

	/*
	 * Disable the timer during suspend.  The timer will not lose
	 * its state in S1 or S2, but we are required to disable
	 * it.
	 */
	sc = device_get_softc(dev);
	hpet_disable(sc);

	return (0);
}

static int
acpi_hpet_resume(device_t dev)
{
	struct acpi_hpet_softc *sc;

	/* Re-enable the timer after a resume to keep the clock advancing. */
	sc = device_get_softc(dev);
	hpet_enable(sc);

	return (0);
}

/* Print some basic latency/rate information to assist in debugging. */
static void
acpi_hpet_test(struct acpi_hpet_softc *sc)
{
	int i;
	uint32_t u1, u2;
	struct bintime b0, b1, b2;
	struct timespec ts;

	binuptime(&b0);
	binuptime(&b0);
	binuptime(&b1);
	u1 = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	for (i = 1; i < 1000; i++)
		u2 = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);
	binuptime(&b2);
	u2 = bus_read_4(sc->mem_res, HPET_MAIN_COUNTER);

	bintime_sub(&b2, &b1);
	bintime_sub(&b1, &b0);
	bintime_sub(&b2, &b1);
	bintime2timespec(&b2, &ts);

	device_printf(sc->dev, "%ld.%09ld: %u ... %u = %u\n",
	    (long)ts.tv_sec, ts.tv_nsec, u1, u2, u2 - u1);

	device_printf(sc->dev, "time per call: %ld ns\n", ts.tv_nsec / 1000);
}

static device_method_t acpi_hpet_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify, acpi_hpet_identify),
	DEVMETHOD(device_probe, acpi_hpet_probe),
	DEVMETHOD(device_attach, acpi_hpet_attach),
	DEVMETHOD(device_detach, acpi_hpet_detach),
	DEVMETHOD(device_suspend, acpi_hpet_suspend),
	DEVMETHOD(device_resume, acpi_hpet_resume),

	{0, 0}
};

static driver_t	acpi_hpet_driver = {
	"acpi_hpet",
	acpi_hpet_methods,
	sizeof(struct acpi_hpet_softc),
};


DRIVER_MODULE(acpi_hpet, acpi, acpi_hpet_driver, acpi_hpet_devclass, 0, 0);
MODULE_DEPEND(acpi_hpet, acpi, 1, 1, 1);
