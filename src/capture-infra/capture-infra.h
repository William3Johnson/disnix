/*
 * Disnix - A Nix-based distributed service deployment tool
 * Copyright (C) 2008-2018  Sander van der Burg
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
#ifndef __DISNIX_CAPTURE_INFRA_H
#define __DISNIX_CAPTURE_INFRA_H
#include <glib.h>

/**
 * Iterates over targets defined in an infrastructure Nix expression and
 * captures the machine configuration from each target.
 *
 * @param interface Path to the client interface executable
 * @param target_property Property in the infrastructure model which specifies
 *                        how to connect to the Disnix service
 * @param infrastructure_expr Path to the infrastructure expression
 * @param xml If set to TRUE it considers the input to be in XML format
 * @return 0 if everything succeeds, else a non-zero exit value
 */
int capture_infra(gchar *interface, gchar *target_property, gchar *infrastructure_expr, const int xml);

#endif
