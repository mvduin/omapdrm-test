#pragma once
#include <utility>
#include <libudev.h>
#include <stddef.h>

inline struct udev *udev_get() {
	static struct udev *singleton = udev_new();
	return singleton;
}

struct udev_list_entry {
	struct udev_list_entry *next() {
		return udev_list_entry_get_next( this );
	}
	char const *name() {  return udev_list_entry_get_name( this );  }
	char const *value() {  return udev_list_entry_get_value( this );  }
};

class UdevEnumerate {
	struct udev_enumerate *e = NULL;

public:
	explicit UdevEnumerate( char const *subsystem = NULL ) {
		e = udev_enumerate_new( udev_get() );
		udev_enumerate_add_match_subsystem( e, subsystem );
	}
	~UdevEnumerate() {
		udev_enumerate_unref( e );
	}

	int match_property( char const *key, char const *value ) {
		return udev_enumerate_add_match_property( e, key, value );
	}

	class Iterator {
		struct udev_list_entry *i;
	public:
		Iterator( struct udev_list_entry *list ) : i( list ) {}
		Iterator( Iterator const & ) = default;

		operator struct udev_list_entry * () {  return i;  }

		Iterator &operator ++ () & {  i = i->next();  return *this;  }

		char const *operator * () {  return i->name();  }
	};

	Iterator begin() {
		udev_enumerate_scan_devices( e );
		return udev_enumerate_get_list_entry( e );
	}
	Iterator end() {
		return NULL;
	}
};

class UdevDevice {
	struct udev_device *d;

public:
	explicit UdevDevice( char const *syspath ) {
		d = udev_device_new_from_syspath( udev_get(), syspath );
	}
	UdevDevice( UdevDevice const &old ) {
		d = udev_device_ref( old.d );
	}
	~UdevDevice() {
		udev_device_unref( d );
	}

	operator bool () {  return d && udev_device_get_is_initialized( d ) > 0;  }

	char const *devpath() {  return udev_device_get_devnode( d );  }
};
