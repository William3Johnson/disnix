/*
 * Disnix - A distributed application layer for Nix
 * Copyright (C) 2008-2010  Sander van der Burg
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

#ifndef __ACTIVATE_H
#define __ACTIVATE_H
#include <glib.h>

/**
 * Activates a distributed system deployment state defined in a manifest file.
 * In this process all the obsolete services from the previous deployment
 * configuration are deactivated and new services from the desired configuration
 * are activated, without breaking inter-dependencies and taking the right
 * order of activation into account. Moreover, before the activation
 * all services receive a lock request which they can use for e.g. blocking,
 * which is released after the process is completed.
 *
 * @param interface Path to the client interface executable
 * @param infrastructure Path to the infrastructure expression
 * @param new_manifest Manifest file representing the new deployment configuration
 * @param new_manifest Manifest file representing the old deployment configuration
 * @param coordinator_profile_path Path where the current deployment state is stored for future reference
 * @param profile Name of the distributed profile
 * @param target_property Property in the infrastructure model which specifies how to connect to the Disnix service
 * @return 0 if the process suceeds, else a non-zero exit value
 */
int activate_system(gchar *interface, gchar *infrastructure, gchar *new_manifest, gchar *old_manifest, gchar *coordinator_profile_path, gchar *profile, gchar *target_property);

#endif
