/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2011  Nokia Corporation
 *  Copyright (C) 2011  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdbool.h>

#include <glib.h>
#include <gdbus/gdbus.h>

#include "lib/uuid.h"
#include "adapter.h"
#include "device.h"
#include "profile.h"
#include "service.h"
#include "gatt.h"
#include "monitor.h"
#include "reporter.h"
#include "manager.h"

static struct enabled enabled  = {
	.linkloss = TRUE,
	.pathloss = TRUE,
	.findme = TRUE,
};

static int monitor_linkloss_probe(struct btd_service *service)
{
	struct btd_device *device = btd_service_get_device(service);
	struct btd_adapter *adapter = device_get_adapter(device);
	int ret;

	ret = monitor_register_linkloss(device, &enabled);

	if (enabled.linkloss && ret == 0)
		btd_adapter_set_auto_connectable(adapter, device, TRUE);

	return ret;
}

static int monitor_immediate_probe(struct btd_service *service)
{
	struct btd_device *device = btd_service_get_device(service);

	return monitor_register_immediate(device, &enabled);
}

static void monitor_linkloss_remove(struct btd_service *service)
{
	struct btd_device *device = btd_service_get_device(service);
	struct btd_adapter *adapter = device_get_adapter(device);

	monitor_unregister_linkloss(device);

	if (enabled.linkloss)
		btd_adapter_set_auto_connectable(adapter, device, FALSE);
}

static void monitor_immediate_remove(struct btd_service *service)
{
	struct btd_device *device = btd_service_get_device(service);

	monitor_unregister_immediate(device);
}

static struct btd_profile pxp_monitor_linkloss_profile = {
	.name		= "proximity-linkloss",
	.remote_uuid	= LINK_LOSS_UUID,
	.device_probe	= monitor_linkloss_probe,
	.device_remove	= monitor_linkloss_remove,
	.connect	= btd_gatt_connect,
	.disconnect	= btd_gatt_disconnect,
	.auto_connect	= true,
};

static struct btd_profile pxp_monitor_immediate_profile = {
	.name		= "proximity-immediate",
	.remote_uuid	= IMMEDIATE_ALERT_UUID,
	.device_probe	= monitor_immediate_probe,
	.device_remove	= monitor_immediate_remove,
	.connect	= btd_gatt_connect,
	.disconnect	= btd_gatt_disconnect,
	.auto_connect	= false,
};

static struct btd_profile pxp_reporter_profile = {
	.name		= "Proximity Reporter GATT Driver",
	.remote_uuid	= GATT_UUID,
	.device_probe	= reporter_probe,
	.device_remove	= reporter_remove,
	.connect	= btd_gatt_connect,
	.disconnect	= btd_gatt_disconnect,
	.auto_connect	= true,
};

static void load_config_file(GKeyFile *config)
{
	char **list;
	int i;

	if (config == NULL)
		return;

	list = g_key_file_get_string_list(config, "General", "Disable",
								NULL, NULL);
	for (i = 0; list && list[i] != NULL; i++) {
		if (g_str_equal(list[i], "FindMe"))
			enabled.findme = FALSE;
		else if (g_str_equal(list[i], "LinkLoss"))
			enabled.linkloss = FALSE;
		else if (g_str_equal(list[i], "PathLoss"))
			enabled.pathloss = FALSE;
	}

	g_strfreev(list);
}

int proximity_manager_init(GKeyFile *config)
{
	load_config_file(config);

	if (btd_profile_register(&pxp_monitor_linkloss_profile) < 0)
		goto fail;

	if (btd_profile_register(&pxp_monitor_immediate_profile) < 0)
		goto fail;

	if (btd_profile_register(&pxp_reporter_profile) < 0)
		goto fail;

	reporter_init();

	return 0;

fail:
	proximity_manager_exit();

	return -1;
}

void proximity_manager_exit(void)
{
	btd_profile_unregister(&pxp_reporter_profile);
	btd_profile_unregister(&pxp_monitor_immediate_profile);
	btd_profile_unregister(&pxp_monitor_linkloss_profile);

	reporter_exit();
}
