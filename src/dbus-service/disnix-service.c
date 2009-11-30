/*
 * Disnix - A distributed application layer for Nix
 * Copyright (C) 2008-2009  Sander van der Burg
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <stdlib.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

/* Server settings variables */

/** Directory in which the activation modules can be found */
char *activation_modules_dir;

/** Optional process that should be invoked by the lock method */
char *lock_manager;

/** Optional process that should be invoked by the unlock method */
char *unlock_manager;

/* D-Bus settings */
#include "disnix-instance-def.h"

/* GType declarations and convienence macros */

/**
 * Forward declaration of the function that will return the GType of
 * the Value implementation. Not used in this program
 */
 
GType disnix_object_get_type (void);

/* 
 * Macro for the above. It is common to define macros using the
 * naming convention (seen below) for all GType implementations,
 * and that's why we're going to do that here as well.
 */
#define DISNIX_TYPE_OBJECT              (disnix_object_get_type ())
#define DISNIX_OBJECT(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), DISNIX_TYPE_OBJECT, DisnixObject))
#define DISNIX_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), DISNIX_TYPE_OBJECT, DisnixObjectClass))
#define DISNIX_IS_OBJECT(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), DISNIX_TYPE_OBJECT))
#define DISNIX_IS_OBJECT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), DISNIX_TYPE_OBJECT))
#define DISNIX_OBJECT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), DISNIX_TYPE_OBJECT, DisnixObjectClass))

/* Utility macro to define the value_object GType structure. */
G_DEFINE_TYPE(DisnixObject, disnix_object, G_TYPE_OBJECT)

/*
 * Forward declarations of D-Bus methods.
 * Since the stub generator will reference the functions from a call
 * table, the functions must be declared before the stub is included.
 */
#include "methods.h"

/* Include the generated server code */
#include "disnix-service.h"

/**
 * Initializes a Disnix object instance
 */
static void disnix_object_init(DisnixObject *obj)
{
    g_assert(obj != NULL);
    obj->pid = NULL;    
}

/**
 * Initializes the Disnix class
 */
static void disnix_object_class_init (DisnixObjectClass *klass)
{
    g_assert(klass != NULL);

    /* Create the signals */
    klass->signals[E_FINISH_SIGNAL] = g_signal_new ("finish", /* signal name */
                                                    G_OBJECT_CLASS_TYPE (klass), /* proxy object type */
						    G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED, /* signal flags */
						    0,
						    NULL, NULL,
						    g_cclosure_marshal_VOID__STRING,
						    G_TYPE_NONE, 
						    1, /* Number of parameter types to follow */
						    G_TYPE_STRING /* Parameter types */);
						    
    klass->signals[E_SUCCESS_SIGNAL] = g_signal_new ("success",
                                                    G_OBJECT_CLASS_TYPE (klass),
						    G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
						    0,
						    NULL, NULL,
						    g_cclosure_marshal_VOID__STRING,
						    G_TYPE_NONE,
						    2,
						    G_TYPE_STRING, G_TYPE_STRING);

    klass->signals[E_FAILURE_SIGNAL] = g_signal_new ("failure",
                                                    G_OBJECT_CLASS_TYPE (klass),
						    G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
						    0,
						    NULL, NULL,
						    g_cclosure_marshal_VOID__STRING,
						    G_TYPE_NONE,
						    1,
						    G_TYPE_STRING);
    
    /* Bind this GType into the Glib/D-Bus wrappers */
    dbus_g_object_type_install_info (DISNIX_TYPE_OBJECT, &dbus_glib_disnix_object_info);
}


/**
 * Starts the Disnix D-Bus service
 *
 * @param activation_modules_dir Directory in which the activation modules can be found
 * @param lock_manager Optional process that should be invoked by the lock method
 * @param unlock_manager Optional process that should be invoked by the unlock method
 */

int start_disnix_service(char *activation_modules_dir_arg, char *lock_manager_arg, char *unlock_manager_arg)
{
    /* The D-Bus connection object provided by dbus_glib */
    DBusGConnection *bus;
    
    /* 
     * Representation of the D-Bus value object locally.
     * (this object acts as a proxy for method calls and signal delivery)
     */
    DBusGProxy *proxy;
    
    /* Holds one instance of DisnixObject that will serve all the requests. */
    DisnixObject *object;
    
    /* GLib mainloop that keeps the server running */
    GMainLoop *mainloop;
    
    /* Variables used to store error information */
    GError *error = NULL;
    guint result;
    
    /* Initialize the GType/GObject system */
    g_type_init();
    
    /* Add the server parameters to the global variables */
    activation_modules_dir = activation_modules_dir_arg;
    lock_manager = lock_manager_arg;
    unlock_manager = unlock_manager_arg;
    
    /* Create a GMainloop with initial state of 'not running' (FALSE) */
    mainloop = g_main_loop_new(NULL, FALSE);
    if(mainloop == NULL)
    {
	g_printerr("ERROR: Failed to create the mainloop!\n");
	return 1;
    }
    
    /* Connect to the system bus */
    g_print("Connecting to the system bus\n");
    bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
    if(error != NULL)
    {
	g_printerr("ERROR: Cannot connect to the system bus! Reason: %s\n", error->message);
	g_error_free(error);
	return 1;
    }
    
    /* Create a GLib proxy object */
    
    g_print("Creating a GLib proxy object\n");    
    proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.DBus", /* Name */
 					    "/org/freedesktop/DBus", /* Object path */
					    "org.freedesktop.DBus"); /* Interface */
    if(proxy == NULL)
    {
	g_printerr("Cannot create a proxy object!\n");
	return 1;
    }
    
    /* Attempt to register the well-known name of the server */
    
    g_print("Registering the org.nixos.disnix.Disnix as the well-known name\n");
    
    if (!dbus_g_proxy_call (proxy, "RequestName", &error,
			    G_TYPE_STRING, "org.nixos.disnix.Disnix",
			    G_TYPE_UINT, 0,
			    G_TYPE_INVALID,
			    G_TYPE_UINT, &result,
			    G_TYPE_INVALID))
    {
        g_printerr("D-Bus.RequestName RPC failed! Reason: %s\n", error->message);
        return 1;
    }

    /* Check the result of the registration */
    g_print("RequestName returned: %d\n", result);
    
    if(result != 1)
    {        
        g_printerr("Failed to get the primary well-known name!\n");
        return 1;
    }

    /* Create one single instance */
    g_print("Creating a single Disnix instance\n");
     
    object = g_object_new(DISNIX_TYPE_OBJECT, NULL);    
    if(!object)
    {
        g_printerr("Failed to create one Disnix instance.\n");
        return 1;
    }
    
    /* Register the instance to D-Bus */
    g_print("Registering the Disnix instance to D-Bus\n");
    dbus_g_connection_register_g_object (bus, "/org/nixos/disnix/Disnix", G_OBJECT(object));

    /* Starting the main loop */
    g_print("The Disnix is service is running!\n");
    g_main_loop_run(mainloop);
    
    /* The main loop should not be stopped, but if it does return the exit failure status */
    return EXIT_FAILURE;
}
