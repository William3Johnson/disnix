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

#ifndef __TRANSITION_H
#define __TRANSITION_H
#include <glib.h>

/**
 * Performs the transition phase, in which obsolete services are deactivated and
 * new services are activated.
 *
 * @param interface Path to the client interface executable
 * @param new_activation_mappings Array containing the activation mappings of the new configuration
 * @param old_activation_mappings Array containing the activation mappings of the old configuration
 * @return TRUE if the phase succeeds, else FALSE
 */
gboolean transition(gchar *interface, GArray *new_activation_mappings, GArray *old_activation_mappings);

#endif
