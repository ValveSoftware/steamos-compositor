// Thanks to Aaron Plattner for that snippet
// Public domain

#include <stdio.h>
#include "libudev.h"

int main (int argc, char **argv)
{
	int ret = 1;
	struct udev *udev = udev_new();
	struct udev_enumerate *enumerate = udev_enumerate_new(udev);
	struct udev_list_entry *entry;

	if (argc != 2)
	{
		printf("usage: udev_is_boot_vga DRIVER\n");
		return 1;
	}

	udev_enumerate_add_match_sysattr(enumerate, "boot_vga", "1");
	udev_enumerate_add_match_sysattr(enumerate, "driver", argv[1]);
	udev_enumerate_scan_devices(enumerate);

	udev_list_entry_foreach(entry, udev_enumerate_get_list_entry(enumerate)) {
		ret = 0;
	}

	udev_enumerate_unref(enumerate);
	udev_unref(udev);

	return ret;
}
