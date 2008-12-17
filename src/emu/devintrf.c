/***************************************************************************

    devintrf.c

    Device interface functions.

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

***************************************************************************/

#include "driver.h"
#include "cpuintrf.h"



/***************************************************************************
    CONSTANTS
***************************************************************************/

#define TEMP_STRING_POOL_ENTRIES		16
#define MAX_STRING_LENGTH				256



/***************************************************************************
    GLOBAL VARIABLES
***************************************************************************/

static char temp_string_pool[TEMP_STRING_POOL_ENTRIES][MAX_STRING_LENGTH];
static int temp_string_pool_index;



/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

static void device_list_stop(running_machine *machine);
static void device_list_reset(running_machine *machine);



/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    get_temp_string_buffer - return a pointer to
    a temporary string buffer
-------------------------------------------------*/

INLINE char *get_temp_string_buffer(void)
{
	char *string = &temp_string_pool[temp_string_pool_index++ % TEMP_STRING_POOL_ENTRIES][0];
	string[0] = 0;
	return string;
}


/*-------------------------------------------------
    device_matches_type - does a device match
    the provided type, taking wildcards into
    effect?
-------------------------------------------------*/

INLINE int device_matches_type(const device_config *device, device_type type)
{
	return (type == DEVICE_TYPE_WILDCARD) ? TRUE : (device->type == type);
}



/***************************************************************************
    DEVICE CONFIGURATION
***************************************************************************/

/*-------------------------------------------------
    device_list_add - add a new device to the
    end of a device list
-------------------------------------------------*/

device_config *device_list_add(device_config **listheadptr, device_type type, const char *tag)
{
	device_config **devptr, **typedevptr;
	device_config *device, *typedevice;
	UINT32 configlen;

	assert(listheadptr != NULL);
	assert(type != NULL);
	assert(tag != NULL);

	/* find the end of the list, and ensure no duplicates along the way */
	for (devptr = listheadptr; *devptr != NULL; devptr = &(*devptr)->next)
		if (type == (*devptr)->type && strcmp(tag, (*devptr)->tag) == 0)
			fatalerror("Attempted to add duplicate device: type=%s tag=%s\n", devtype_get_name(type), tag);

	/* get the size of the inline config */
	configlen = (UINT32)devtype_get_info_int(type, DEVINFO_INT_INLINE_CONFIG_BYTES);

	/* allocate a new device */
	device = malloc_or_die(sizeof(*device) + strlen(tag) + configlen);

	/* populate all fields */
	device->next = NULL;
	device->typenext = NULL;
	device->type = type;
	device->class = devtype_get_info_int(type, DEVINFO_INT_CLASS);
	device->static_config = NULL;
	device->inline_config = (configlen == 0) ? NULL : (device->tag + strlen(tag) + 1);
	device->started = FALSE;
	device->token = NULL;
	device->machine = NULL;
	device->region = NULL;
	device->regionbytes = 0;
	strcpy(device->tag, tag);

	/* reset the inline_config to 0 */
	if (configlen > 0)
		memset(device->inline_config, 0, configlen);

	/* fetch function pointers to the core functions */
	device->set_info = (device_set_info_func)devtype_get_info_fct(type, DEVINFO_FCT_SET_INFO);

	/* before adding us to the global list, add us to the end of the class list */
	typedevice = (device_config *)device_list_first(*listheadptr, type);
	for (typedevptr = &typedevice; *typedevptr != NULL; typedevptr = &(*typedevptr)->typenext) ;
	*typedevptr = device;

	/* link us to the end and return */
	*devptr = device;
	return device;
}


/*-------------------------------------------------
    device_list_remove - remove a device from a
    device list
-------------------------------------------------*/

void device_list_remove(device_config **listheadptr, device_type type, const char *tag)
{
	device_config **devptr, **typedevptr;
	device_config *device, *typedevice;

	assert(listheadptr != NULL);
	assert(type != NULL);
	assert(tag != NULL);

	/* find the device in the list */
	for (devptr = listheadptr; *devptr != NULL; devptr = &(*devptr)->next)
		if (type == (*devptr)->type && strcmp(tag, (*devptr)->tag) == 0)
			break;
	device = *devptr;
	if (device == NULL)
		fatalerror("Attempted to remove non-existant device: type=%s tag=%s\n", devtype_get_name(type), tag);

	/* before removing us from the global list, remove us from the type list */
	typedevice = (device_config *)device_list_first(*listheadptr, type);
	for (typedevptr = &typedevice; *typedevptr != device; typedevptr = &(*typedevptr)->typenext) ;
	assert(*typedevptr == device);
	*typedevptr = device->typenext;

	/* remove the device from the list */
	*devptr = device->next;

	/* free the device object */
	free(device);
}


/*-------------------------------------------------
    device_build_tag - build a tag that combines
    the device's name and the given tag
-------------------------------------------------*/

const char *device_build_tag(astring *dest, const device_config *device, const char *tag)
{
	if (device != NULL)
	{
		astring_cpyc(dest, device->tag);
		astring_catc(dest, ":");
		astring_catc(dest, tag);
	}
	else
		astring_cpyc(dest, tag);
	return astring_c(dest);
}


/*-------------------------------------------------
    device_inherit_tag - build a tag with the same
    device prefix as the source tag
-------------------------------------------------*/

const char *device_inherit_tag(astring *dest, const char *sourcetag, const char *tag)
{
	const char *divider = strrchr(sourcetag, ':');
	if (divider != NULL)
	{
		astring_cpych(dest, sourcetag, divider + 1 - sourcetag);
		astring_catc(dest, tag);
	}
	else
		astring_cpyc(dest, tag);
	return astring_c(dest);
}



/***************************************************************************
    TYPE-BASED DEVICE ACCESS
***************************************************************************/

/*-------------------------------------------------
    device_list_items - return the number of
    items of a given type; DEVICE_TYPE_WILDCARD
    is allowed
-------------------------------------------------*/

int device_list_items(const device_config *listhead, device_type type)
{
	const device_config *curdev;
	int count = 0;

	assert(type != NULL);

	/* locate all devices */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		count += device_matches_type(curdev, type);

	return count;
}


/*-------------------------------------------------
    device_list_first - return the first device
    in the list of a given type;
    DEVICE_TYPE_WILDCARD is allowed
-------------------------------------------------*/

const device_config *device_list_first(const device_config *listhead, device_type type)
{
	const device_config *curdev;

	/* scan forward starting with the list head */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		if (device_matches_type(curdev, type))
			return curdev;

	return NULL;
}


/*-------------------------------------------------
    device_list_next - return the next device
    in the list of a given type;
    DEVICE_TYPE_WILDCARD is allowed
-------------------------------------------------*/

const device_config *device_list_next(const device_config *prevdevice, device_type type)
{
	assert(prevdevice != NULL);
	return (type == DEVICE_TYPE_WILDCARD) ? prevdevice->next : prevdevice->typenext;
}


/*-------------------------------------------------
    device_list_find_by_tag - retrieve a device
    configuration based on a type and tag
-------------------------------------------------*/

const device_config *device_list_find_by_tag(const device_config *listhead, device_type type, const char *tag)
{
	const device_config *curdev;

	assert(tag != NULL);

	/* find the device in the list */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		if (device_matches_type(curdev, type) && strcmp(tag, curdev->tag) == 0)
			return curdev;

	/* fail */
	return NULL;
}


/*-------------------------------------------------
    device_list_index - return the index of a
    device based on its type and tag;
    DEVICE_TYPE_WILDCARD is allowed
-------------------------------------------------*/

int device_list_index(const device_config *listhead, device_type type, const char *tag)
{
	const device_config *curdev;
	int index = 0;

	assert(type != NULL);
	assert(tag != NULL);

	/* locate all devices */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		if (device_matches_type(curdev, type))
		{
			if (strcmp(tag, curdev->tag) == 0)
				return index;
			index++;
		}

	return -1;
}


/*-------------------------------------------------
    device_list_find_by_index - retrieve a device
    configuration based on a type and index
-------------------------------------------------*/

const device_config *device_list_find_by_index(const device_config *listhead, device_type type, int index)
{
	const device_config *curdev;

	assert(type != NULL);
	assert(index >= 0);

	/* find the device in the list */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		if (device_matches_type(curdev, type) && index-- == 0)
			return curdev;

	/* fail */
	return NULL;
}



/***************************************************************************
    CLASS-BASED DEVICE ACCESS
***************************************************************************/

/*-------------------------------------------------
    device_list_class_items - return the number of
    items of a given class
-------------------------------------------------*/

int device_list_class_items(const device_config *listhead, device_class class)
{
	const device_config *curdev;
	int count = 0;

	/* locate all devices */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		count += (curdev->class == class);

	return count;
}


/*-------------------------------------------------
    device_list_class_first - return the first
    device in the list of a given class
-------------------------------------------------*/

const device_config *device_list_class_first(const device_config *listhead, device_class class)
{
	const device_config *curdev;

	/* scan forward starting with the list head */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		if (curdev->class == class)
			return curdev;

	return NULL;
}


/*-------------------------------------------------
    device_list_class_next - return the next
    device in the list of a given class
-------------------------------------------------*/

const device_config *device_list_class_next(const device_config *prevdevice, device_class class)
{
	const device_config *curdev;

	assert(prevdevice != NULL);

	/* scan forward starting with the item after the previous one */
	for (curdev = prevdevice->next; curdev != NULL; curdev = curdev->next)
		if (curdev->class == class)
			return curdev;

	return NULL;
}


/*-------------------------------------------------
    device_list_class_find_by_tag - retrieve a
    device configuration based on a class and tag
-------------------------------------------------*/

const device_config *device_list_class_find_by_tag(const device_config *listhead, device_class class, const char *tag)
{
	const device_config *curdev;

	assert(tag != NULL);

	/* find the device in the list */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		if (curdev->class == class && strcmp(tag, curdev->tag) == 0)
			return curdev;

	/* fail */
	return NULL;
}


/*-------------------------------------------------
    device_list_class_index - return the index of a
    device based on its class and tag
-------------------------------------------------*/

int device_list_class_index(const device_config *listhead, device_class class, const char *tag)
{
	const device_config *curdev;
	int index = 0;

	assert(tag != NULL);

	/* locate all devices */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		if (curdev->class == class)
		{
			if (strcmp(tag, curdev->tag) == 0)
				return index;
			index++;
		}

	return -1;
}


/*-------------------------------------------------
    device_list_class_find_by_index - retrieve a
    device configuration based on a class and
    index
-------------------------------------------------*/

const device_config *device_list_class_find_by_index(const device_config *listhead, device_class class, int index)
{
	const device_config *curdev;

	assert(index >= 0);

	/* find the device in the list */
	for (curdev = listhead; curdev != NULL; curdev = curdev->next)
		if (curdev->class == class && index-- == 0)
			return curdev;

	/* fail */
	return NULL;
}



/***************************************************************************
    LIVE DEVICE MANAGEMENT
***************************************************************************/

/*-------------------------------------------------
    device_list_attach_machine - "attach" a 
    running_machine to its list of devices
-------------------------------------------------*/

void device_list_attach_machine(running_machine *machine)
{
	device_config *device;

	assert(machine != NULL);

	/* iterate over devices and assign the machine to them */
	for (device = (device_config *)machine->config->devicelist; device != NULL; device = device->next)
		device->machine = machine;
}


/*-------------------------------------------------
    device_list_start - start the configured list
    of devices for a machine
-------------------------------------------------*/

void device_list_start(running_machine *machine)
{
	device_config *device;
	int numstarted = 0;
	int devcount = 0;

	assert(machine != NULL);

	/* add an exit callback for cleanup */
	add_reset_callback(machine, device_list_reset);
	add_exit_callback(machine, device_list_stop);

	/* iterate over devices and allocate memory for them */
	for (device = (device_config *)machine->config->devicelist; device != NULL; device = device->next)
	{
		deviceinfo info = { 0 };

		assert(!device->started);
		assert(device->machine == machine);
		assert(device->token == NULL);
		assert(device->type != NULL);

		devcount++;

		/* get the size of the token data; we do it directly because we can't call device_get_info_* with no token */
		(*device->type)(device, DEVINFO_INT_TOKEN_BYTES, &info);
		device->tokenbytes = (UINT32)info.i;
		if (device->tokenbytes == 0)
			fatalerror("Device %s specifies a 0 token length!\n", device_get_name(device));

		/* allocate memory for the token */
		device->token = malloc_or_die(device->tokenbytes);
		memset(device->token, 0, device->tokenbytes);

		/* fill in the remaining runtime fields */
		device->machine = machine;
		device->region = memory_region(machine, device->tag);
		device->regionbytes = memory_region_length(machine, device->tag);
	}

	/* iterate until we've started everything */
	while (numstarted < devcount)
	{
		int prevstarted = numstarted;
		numstarted = 0;

		/* iterate over devices and start them */
		for (device = (device_config *)machine->config->devicelist; device != NULL; device = device->next)
		{
			device_start_func start = (device_start_func)device_get_info_fct(device, DEVINFO_FCT_START);

			assert(start != NULL);
			if (!device->started && (*start)(device) == DEVICE_START_OK)
				device->started = TRUE;
			numstarted += device->started;
		}

		/* if we didn't start anything new, we're in trouble */
		if (numstarted == prevstarted)
			fatalerror("Circular dependency in device startup; unable to start %d/%d devices\n", devcount - numstarted, devcount);
	}
}


/*-------------------------------------------------
    device_list_stop - stop the configured list
    of devices for a machine
-------------------------------------------------*/

static void device_list_stop(running_machine *machine)
{
	device_config *device;

	assert(machine != NULL);

	/* iterate over devices and stop them */
	for (device = (device_config *)machine->config->devicelist; device != NULL; device = device->next)
	{
		device_stop_func stop = (device_stop_func)device_get_info_fct(device, DEVINFO_FCT_STOP);

		assert(device->token != NULL);
		assert(device->type != NULL);

		/* if we have a stop function, call it */
		if (stop != NULL)
			(*stop)(device);

		/* free allocated memory for the token */
		if (device->token != NULL)
			free(device->token);

		/* reset all runtime fields */
		device->token = NULL;
		device->tokenbytes = 0;
		device->machine = NULL;
		device->region = NULL;
		device->regionbytes = 0;
	}
}


/*-------------------------------------------------
    device_list_reset - reset the configured list
    of devices for a machine
-------------------------------------------------*/

static void device_list_reset(running_machine *machine)
{
	const device_config *device;

	assert(machine != NULL);

	/* iterate over devices and stop them */
	for (device = (device_config *)machine->config->devicelist; device != NULL; device = device->next)
		device_reset(device);
}


/*-------------------------------------------------
    device_reset - reset a device based on an
    allocated device_config
-------------------------------------------------*/

void device_reset(const device_config *device)
{
	device_reset_func reset;

	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type != NULL);

	/* if we have a reset function, call it */
	reset = (device_reset_func)device_get_info_fct(device, DEVINFO_FCT_RESET);
	if (reset != NULL)
		(*reset)(device);
}



/***************************************************************************
    DEVICE INFORMATION GETTERS
***************************************************************************/

/*-------------------------------------------------
    device_get_info_int - return an integer state
    value from an allocated device
-------------------------------------------------*/

INT64 device_get_info_int(const device_config *device, UINT32 state)
{
	deviceinfo info;

	assert(device != NULL);
	assert(device->type != NULL);
	assert(state >= DEVINFO_INT_FIRST && state <= DEVINFO_INT_LAST);

	/* retrieve the value */
	info.i = 0;
	(*device->type)(device, state, &info);
	return info.i;
}


/*-------------------------------------------------
    device_get_info_ptr - return a pointer state
    value from an allocated device
-------------------------------------------------*/

void *device_get_info_ptr(const device_config *device, UINT32 state)
{
	deviceinfo info;

	assert(device != NULL);
	assert(device->type != NULL);
	assert(state >= DEVINFO_PTR_FIRST && state <= DEVINFO_PTR_LAST);

	/* retrieve the value */
	info.p = NULL;
	(*device->type)(device, state, &info);
	return info.p;
}


/*-------------------------------------------------
    device_get_info_fct - return a function
    pointer state value from an allocated device
-------------------------------------------------*/

genf *device_get_info_fct(const device_config *device, UINT32 state)
{
	deviceinfo info;

	assert(device != NULL);
	assert(device->type != NULL);
	assert(state >= DEVINFO_FCT_FIRST && state <= DEVINFO_FCT_LAST);

	/* retrieve the value */
	info.f = 0;
	(*device->type)(device, state, &info);
	return info.f;
}


/*-------------------------------------------------
    device_get_info_string - return a string value
    from an allocated device
-------------------------------------------------*/

const char *device_get_info_string(const device_config *device, UINT32 state)
{
	deviceinfo info;

	assert(device != NULL);
	assert(device->type != NULL);
	assert(state >= DEVINFO_STR_FIRST && state <= DEVINFO_STR_LAST);

	/* retrieve the value */
	info.s = get_temp_string_buffer();
	(*device->type)(device, state, &info);
	return info.s;
}



/***************************************************************************
    DEVICE TYPE INFORMATION SETTERS
***************************************************************************/

/*-------------------------------------------------
    devtype_get_info_int - return an integer value
    from a device type (does not need to be
    allocated)
-------------------------------------------------*/

INT64 devtype_get_info_int(device_type type, UINT32 state)
{
	deviceinfo info;

	assert(type != NULL);
	assert(state >= DEVINFO_INT_FIRST && state <= DEVINFO_INT_LAST);

	/* retrieve the value */
	info.i = 0;
	(*type)(NULL, state, &info);
	return info.i;
}


/*-------------------------------------------------
    devtype_get_info_int - return a function 
    pointer from a device type (does not need to 
    be allocated)
-------------------------------------------------*/

genf *devtype_get_info_fct(device_type type, UINT32 state)
{
	deviceinfo info;

	assert(type != NULL);
	assert(state >= DEVINFO_FCT_FIRST && state <= DEVINFO_FCT_LAST);

	/* retrieve the value */
	info.f = 0;
	(*type)(NULL, state, &info);
	return info.f;
}


/*-------------------------------------------------
    devtype_get_info_string - return a string value
    from a device type (does not need to be
    allocated)
-------------------------------------------------*/

const char *devtype_get_info_string(device_type type, UINT32 state)
{
	deviceinfo info;

	assert(type != NULL);
	assert(state >= DEVINFO_STR_FIRST && state <= DEVINFO_STR_LAST);

	/* retrieve the value */
	info.s = get_temp_string_buffer();
	(*type)(NULL, state, &info);
	return info.s;
}



/***************************************************************************
    DEVICE INFORMATION SETTERS
***************************************************************************/

/*-------------------------------------------------
    device_set_info_int - set an integer state
    value for an allocated device
-------------------------------------------------*/

void device_set_info_int(const device_config *device, UINT32 state, INT64 data)
{
	deviceinfo info;

	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type != NULL);
	assert(state >= DEVINFO_INT_FIRST && state <= DEVINFO_INT_LAST);

	/* set the value */
	info.i = data;
	(*device->set_info)(device, state, &info);
}


/*-------------------------------------------------
    device_set_info_ptr - set a pointer state
    value for an allocated device
-------------------------------------------------*/

void device_set_info_ptr(const device_config *device, UINT32 state, void *data)
{
	deviceinfo info;

	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type != NULL);
	assert(state >= DEVINFO_PTR_FIRST && state <= DEVINFO_PTR_LAST);

	/* set the value */
	info.p = data;
	(*device->set_info)(device, state, &info);
}


/*-------------------------------------------------
    device_set_info_fct - set a function pointer
    state value for an allocated device
-------------------------------------------------*/

void device_set_info_fct(const device_config *device, UINT32 state, genf *data)
{
	deviceinfo info;

	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type != NULL);
	assert(state >= DEVINFO_FCT_FIRST && state <= DEVINFO_FCT_LAST);

	/* set the value */
	info.f = data;
	(*device->set_info)(device, state, &info);
}
