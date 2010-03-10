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

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <glib.h>
#include "signals.h"
#define BUFFER_SIZE 1024

extern char *activation_modules_dir;

static int job_counter = 0;

static gchar *generate_derivations_string(gchar **derivation, char *separator)
{
    unsigned int i;
    gchar *derivations_string = g_strdup("");
    
    for(i = 0; i < g_strv_length(derivation); i++)
    {
	gchar *old_derivations_string = derivations_string;
	derivations_string = g_strconcat(old_derivations_string, separator, derivation[i], NULL);
	g_free(old_derivations_string);
    }
    
    return derivations_string;
}

static gint *generate_int_key(gint *key)
{
    gint *ret = (gint*)g_malloc(sizeof(gint));
    *ret = *key;
    return ret;
}

gchar **update_lines_vector(gchar **lines, char *buf)
{
    unsigned int lines_length;
    gchar **additional_lines = g_strsplit(buf, "\n", 0); /* Split the buffer by newlines */
    unsigned int additional_lines_length = g_strv_length(additional_lines);
    unsigned int i;
    
    if(lines == NULL)
    {
	lines = (gchar**)g_malloc(sizeof(gchar*));
	lines[0] = NULL;
	lines_length = 0;
    }
    else
	lines_length = g_strv_length(lines);
    
    if(lines_length == 0)
    {
	lines = (gchar**)g_realloc(lines, (additional_lines_length + 1) * sizeof(gchar*));
	lines[0] = additional_lines[0];
    }
    else if(strlen(lines[lines_length - 1]) > 0 && lines[lines_length - 1][strlen(lines[lines_length - 1]) - 1] == '\n')
    {
	/* Increase length */
	lines_length++;
	
	/* Increase the allocated memory */
	lines = (gchar**)g_realloc(lines, (lines_length + additional_lines_length + 1) * sizeof(gchar*));
	
	/* Add first additional line to the end */ 
	lines[lines_length - 1] = additional_lines[0];
    }
    else
    {
        gchar *old_last_line, *new_last_line;
	
        /** Increase the allocated memory */
        lines = (gchar**)g_realloc(lines, (lines_length + additional_lines_length + 1) * sizeof(gchar*));
    
        /* Append first addtional line to the end of the last line */
        old_last_line = lines[lines_length - 1];
        new_last_line = g_strconcat(old_last_line, additional_lines[0], NULL);
        lines[lines_length - 1] = new_last_line;
        g_free(old_last_line);
        g_free(additional_lines[0]);
    }
    
    /* Add the other additional lines to the end of the lines vector */
    for(i = 1; i < additional_lines_length; i++)
        lines[lines_length + i] = additional_lines[i];
    
    /* Add NULL termination */
    lines[lines_length + additional_lines_length] = NULL;
    
    /* Clean up additional lines vector */
    g_free(additional_lines);
    
    return lines;
}

/* Import method */

static void disnix_import_thread_func(DisnixObject *object, const gint pid, gchar *closure)
{
    /* Declarations */
    int closure_fd;
    
    /* Print log entry */
    g_print("Importing: %s\n", closure);
    
    /* Execute command */
    closure_fd = open(closure, O_RDONLY);
    
    if(closure_fd == -1)
    {
	g_printerr("Cannot open closure file!\n");
	disnix_emit_failure_signal(object, pid);
    }
    else
    {
	int status = fork();
    
	if(status == 0)
	{
	    char *args[] = {"nix-store", "--import", NULL};
	    dup2(closure_fd, 0);
	    execvp("nix-store", args);
	    _exit(1);
	}
    
	if(status == -1)
	{
	    g_printerr("Error with forking nix-store process!\n");
	    disnix_emit_failure_signal(object, pid);
	}
	else
	{
	    wait(&status);
	
	    if(WEXITSTATUS(status) == 0)
		disnix_emit_finish_signal(object, pid);
	    else
		disnix_emit_failure_signal(object, pid);
	}
    
	close(closure_fd);
    }
    
    _exit(0);
}

gboolean disnix_import(DisnixObject *object, const gint pid, gchar *closure, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);
    
    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_import_thread_func(object, pid, closure);
    
    return TRUE;
}

/* Export method */

static void disnix_export_thread_func(DisnixObject *object, const gint pid, gchar **derivation)
{
    /* Declarations */
    gchar *derivations_string;
    char line[BUFFER_SIZE];
    char tempfilename[19] = "/tmp/disnix.XXXXXX";
    int closure_fd;
    
    /* Generate derivations string */
    derivations_string = generate_derivations_string(derivation, " ");
    
    /* Print log entry */
    g_print("Exporting: %s\n", derivations_string);
    
    /* Execute command */
        
    closure_fd = mkstemp(tempfilename);
    
    if(closure_fd == -1)
    {
	g_printerr("Error opening tempfile!\n");
	disnix_emit_failure_signal(object, pid);
    }
    else
    {
	int status = fork();
    
	if(status == 0)
	{
	    unsigned int i, derivation_length = g_strv_length(derivation);
	    char **args = (char**)g_malloc((3 + derivation_length) * sizeof(char*));
	
	    args[0] = "nix-store";
	    args[1] = "--export";
	
	    for(i = 0; i < derivation_length; i++)
		args[i + 2] = derivation[i];
	
	    args[i + 2] = NULL;
	
	    dup2(closure_fd, 1);
	    execvp("nix-store", args);
	    _exit(1);
	}
	
	if(status == -1)
	{
	    g_printerr("Error with forking nix-store process!\n");
	    disnix_emit_failure_signal(object, pid);
	}
	else
	{
	    wait(&status);
	    
	    if(WEXITSTATUS(status) == 0)
	    {
		gchar *tempfilepaths[2];
		tempfilepaths[0] = tempfilename;
		tempfilepaths[1] = NULL;
		disnix_emit_success_signal(object, pid, tempfilepaths);
	    }
	    else
		disnix_emit_failure_signal(object, pid);
	}
	
	close(closure_fd);
    }
    
    /* Free variables */    
    g_free(derivations_string);
    
    _exit(0);
}

gboolean disnix_export(DisnixObject *object, const gint pid, gchar **derivation, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);

    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_export_thread_func(object, pid, derivation);
        
    return TRUE;
}

/* Print invalid paths method */

static void disnix_print_invalid_thread_func(DisnixObject *object, const gint pid, gchar **derivation)
{
    /* Declarations */
    gchar *derivations_string;
    int pipefd[2];
    
    /* Generate derivations string */
    derivations_string = generate_derivations_string(derivation, " ");
    
    /* Print log entry */
    g_print("Print invalid: %s\n", derivations_string);
    
    /* Execute command */
    
    if(pipe(pipefd) == 0)
    {
	int status = fork();
	
	if(status == 0)
	{
	    unsigned int i, derivation_length = g_strv_length(derivation);
	    char **args = (char**)g_malloc((4 + derivation_length) * sizeof(char*));
	    
	    close(pipefd[0]); /* Close read-end of the pipe */
	
	    args[0] = "nix-store";
	    args[1] = "--check-validity";
	    args[2] = "--print-invalid";
	
	    for(i = 0; i < derivation_length; i++)	
		args[i + 3] = derivation[i];
	    
	    args[i + 3] = NULL;

	    dup2(pipefd[1], 1); /* Attach write-end to stdout */
	    execvp("nix-store", args);
	    _exit(1);
	}
    
	if(status == -1)
	{
	    g_printerr("Error with forking nix-store process!\n");
	    close(pipefd[0]);
	    close(pipefd[1]);
	    disnix_emit_failure_signal(object, pid);
	}
	else
	{
	    char line[BUFFER_SIZE];
	    ssize_t line_size;
	    gchar **missing_paths = NULL;
	    unsigned int missing_paths_size = 0;
	    
	    close(pipefd[1]); /* Close write-end of the pipe */
	    	
	    while((line_size = read(pipefd[0], line, BUFFER_SIZE - 1)) > 0)
	    {
	        line[line_size] = '\0';
	        g_print("%s", line);
				
		missing_paths = update_lines_vector(missing_paths, line);		
	    }
	
	    g_print("\n");
	    
	    close(pipefd[0]);
	    
	    wait(&status);
	
	    if(WEXITSTATUS(status) == 0)
		disnix_emit_success_signal(object, pid, missing_paths);
	    else
		disnix_emit_failure_signal(object, pid);
	    
	    g_strfreev(missing_paths);
	}
    }
    else
    {
	fprintf(stderr, "Error with creating a pipe!\n");
	disnix_emit_failure_signal(object, pid);
    }
        
    /* Free variables */
    g_free(derivations_string);
    
    _exit(0);
}

gboolean disnix_print_invalid(DisnixObject *object, const gint pid, gchar **derivation, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);

    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_print_invalid_thread_func(object, pid, derivation);    
        
    return TRUE;
}

/* Realise method */

static void disnix_realise_thread_func(DisnixObject *object, const gint pid, gchar **derivation)
{
    /* Declarations */
    gchar *derivations_string;
    int pipefd[2];
    
    /* Generate derivation strings */
    derivations_string = generate_derivations_string(derivation, " ");
    
    /* Print log entry */
    g_print("Realising: %s\n", derivations_string);
    
    /* Execute command */    

    if(pipe(pipefd) == 0)
    {
	int status = fork();
	
	if(status == 0)
	{
	    unsigned int i, derivation_size = g_strv_length(derivation);
	    char **args = (char**)g_malloc((3 + derivation_size) * sizeof(char*));
	    
	    close(pipefd[0]); /* Close read-end of pipe */
	    
	    args[0] = "nix-store";
	    args[1] = "-r";
	    
	    for(i = 0; i < derivation_size; i++)
		args[i + 2] = derivation[i];
	
	    args[i + 2] = NULL;
	    
	    dup2(pipefd[1], 1);
	    execvp("nix-store", args);
	    _exit(1);
	}
	
	if(status == -1)
	{
	    g_printerr("Error with forking nix-store process!\n");
	    disnix_emit_failure_signal(object, pid);
	}
	else
	{
	    char line[BUFFER_SIZE];
	    ssize_t line_size;
	    gchar **realised = NULL;
	    unsigned int realised_size = 0;
	
	    close(pipefd[1]); /* Close write-end of pipe */
	    
	    while((line_size = read(pipefd[0], line, BUFFER_SIZE - 1)) > 0)
	    {
	        line[line_size] = '\0';
		g_print("%s", line);
		realised = update_lines_vector(realised, line);
	    }

	    g_print("\n");
	    
	    close(pipefd[0]);
	    
	    wait(&status);
	    
	    if(WEXITSTATUS(status) == 0)
		disnix_emit_success_signal(object, pid, realised);
	    else
		disnix_emit_failure_signal(object, pid);
	    
	    g_strfreev(realised);
	}
    }
    else
    {
	g_printerr("Error with creating a pipe\n");
	disnix_emit_failure_signal(object, pid);
    }
    
    /* Free variables */
    g_free(derivations_string);
    
    _exit(0);
}

gboolean disnix_realise(DisnixObject *object, const gint pid, gchar **derivation, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);

    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_realise_thread_func(object, pid, derivation);    
    
    return TRUE;
}

/* Set method */

static void disnix_set_thread_func(DisnixObject *object, const gint pid, const gchar *profile, gchar *derivation)
{
    /* Declarations */
    gchar *profile_path;
    int status;
    char line[BUFFER_SIZE];
    
    /* Print log entry */
    g_print("Set profile: %s with derivation: %s\n", profile, derivation);
    
    /* Execute command */
    
    mkdir(LOCALSTATEDIR "/nix/profiles/disnix", 0755);
    profile_path = g_strconcat(LOCALSTATEDIR "/nix/profiles/disnix/", profile, NULL);

    status = fork();    
    
    if(status == -1)
    {
	g_printerr("Error with forking nix-env process!\n");
	disnix_emit_failure_signal(object, pid);
    }
    else if(status == 0)
    {
	char *args[] = {"nix-env", "-p", profile_path, "--set", derivation, NULL};
	execvp("nix-env", args);
	g_printerr("Error with executing nix-env\n");
	_exit(1);
    }
    
    if(status != -1)
    {
	wait(&status);
	
	if(WEXITSTATUS(status) == 0)
	    disnix_emit_finish_signal(object, pid);
	else
	    disnix_emit_failure_signal(object, pid);
    }
    
    /* Free variables */
    g_free(profile_path);
    
    _exit(0);
}

gboolean disnix_set(DisnixObject *object, const gint pid, const gchar *profile, gchar *derivation, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);
    
    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_set_thread_func(object, pid, profile, derivation);
        
    return TRUE;
}

/* Query installed method */

static void disnix_query_installed_thread_func(DisnixObject *object, const gint pid, const gchar *profile)
{
    /* Declarations */
    gchar *cmd;
    FILE *fp;
    
    /* Print log entry */
    g_print("Query installed derivations from profile: %s\n", profile);
    
    /* Execute command */
    
    cmd = g_strconcat(LOCALSTATEDIR "/nix/profiles/disnix/", profile, "/manifest", NULL);

    fp = fopen(cmd, "r");
    if(fp == NULL)
	disnix_emit_failure_signal(object, pid); /* Something went wrong with forking the process */
    else
    {
	int status;
	char line[BUFFER_SIZE];
	gchar **derivation = NULL;
	unsigned int derivation_size = 0;
	
	/* Read the output */
	
	while(fgets(line, sizeof(line), fp) != NULL)
	{
	    puts(line);
	    derivation = (gchar**)g_realloc(derivation, (derivation_size + 1) * sizeof(gchar*));
	    derivation[derivation_size] = g_strdup(line);	    
	    derivation_size++;
	}
	
	/* Add NULL value to the end of the list */
	derivation = (gchar**)g_realloc(derivation, (derivation_size + 1) * sizeof(gchar*));
	derivation[derivation_size] = NULL;
	
	fclose(fp);
	
	disnix_emit_success_signal(object, pid, derivation);
	
	g_strfreev(derivation);
    }
    
    _exit(0);
}

gboolean disnix_query_installed(DisnixObject *object, const gint pid, const gchar *profile, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);
    
    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_query_installed_thread_func(object, pid, profile);
    
    return TRUE;
}

/* Query requisites method */

static void disnix_query_requisites_thread_func(DisnixObject *object, const gint pid, gchar **derivation)
{
    /* Declarations */
    gchar *derivations_string;
    int pipefd[2];
    
    /* Generate derivations string */
    derivations_string = generate_derivations_string(derivation, " ");

    /* Print log entry */
    g_print("Query requisites from derivations: %s\n", derivations_string);
    
    /* Execute command */
    
    if(pipe(pipefd) == 0)
    {
	int status = fork();
	
	if(status == 0)
	{
	    unsigned int i, derivation_size = g_strv_length(derivation);
	    char **args = (char**)g_malloc((3 + derivation_size) * sizeof(char*));
	    
	    close(pipefd[0]); /* Close read-end of pipe */
	    
	    args[0] = "nix-store";
	    args[1] = "-qR";
	    
	    for(i = 0; i < derivation_size; i++)
		args[i + 2] = derivation[i];
	
	    args[i + 2] = NULL;
	    
	    dup2(pipefd[1], 1);
	    execvp("nix-store", args);
	    _exit(1);
	}
	
	if(status == -1)
	{
	    fprintf(stderr, "Error with forking nix-store process!\n");
	    close(pipefd[0]);
	    close(pipefd[1]);
	    disnix_emit_failure_signal(object, pid);
	}
	else
	{
	    char line[BUFFER_SIZE];
	    ssize_t line_size;
	    gchar **requisites = NULL;
	    unsigned int requisites_size = 0;

	    close(pipefd[1]); /* Close write-end of pipe */
	    	
	    while((line_size = read(pipefd[0], line, BUFFER_SIZE - 1)) > 0)
	    {
	        line[line_size] = '\0';
		g_print("%s", line);
		requisites = update_lines_vector(requisites, line);
	    }
		
	    g_print("\n");
	    
	    close(pipefd[0]);

	    wait(&status);
	    
	    if(WEXITSTATUS(status) == 0)
		disnix_emit_success_signal(object, pid, requisites);
	    else
		disnix_emit_failure_signal(object, pid);
	
	    g_strfreev(requisites);
	}
    }
    else
    {
	g_printerr("Error with creating pipe!\n");
	disnix_emit_failure_signal(object, pid);
    }
        
    /* Free variables */
    g_free(derivations_string);
    
    _exit(0);
}

gboolean disnix_query_requisites(DisnixObject *object, const gint pid, gchar **derivation, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);
    
    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_query_requisites_thread_func(object, pid, derivation);
    
    return TRUE;
}

/* Garbage collect method */

static void disnix_collect_garbage_thread_func(DisnixObject *object, const gint pid, const gboolean delete_old)
{
    /* Declarations */
    int status;

    /* Print log entry */
    if(delete_old)
	g_print("Garbage collect and remove old derivations\n");
    else
	g_print("Garbage collect\n");
    
    /* Execute command */
    
    status = fork();
    
    if(status == -1)
    {
	g_printerr("Error with forking garbage collect process!\n");
	disnix_emit_failure_signal(object, pid);
    }
    else if(status == 0)
    {
	if(delete_old)
	{
	    char *args[] = {"nix-collect-garbage", NULL};
	    execvp("nix-collect-garbage", args);
	}
	else
	{
	    char *args[] = {"nix-collect-garbage", "-d", NULL};
	    execvp("nix-collect-garbage", args);
	}
	g_printerr("Error with executing garbage collect process\n");
	_exit(1);
    }
    
    if(status != -1)
    {
	wait(&status);
	
	if(WEXITSTATUS(status) == 0)
	    disnix_emit_finish_signal(object, pid);
	else
	    disnix_emit_failure_signal(object, pid);
    }
        
    _exit(0);
}

gboolean disnix_collect_garbage(DisnixObject *object, const gint pid, const gboolean delete_old, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);

    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_collect_garbage_thread_func(object, pid, delete_old);
    
    return TRUE;
}

/* Activate method */

static void disnix_activate_thread_func(DisnixObject *object, const gint pid, gchar *derivation, const gchar *type, gchar **arguments)
{
    /* Declarations */
    gchar *arguments_string;
    int status;
        
    /* Generate arguments string */
    arguments_string = generate_derivations_string(arguments, " ");
    
    /* Print log entry */
    g_print("Activate: %s of type: %s with arguments: %s\n", derivation, type, arguments_string);
    
    /* Execute command */

    status = fork();
    
    if(status == 0)
    {
	unsigned int i;
	gchar *cmd = g_strconcat(activation_modules_dir, "/", type, NULL);
	char *args[] = {cmd, "activate", derivation, NULL};
	
	for(i = 0; i < g_strv_length(arguments); i++)
	{
	    gchar **name_value_pair = g_strsplit(arguments[i], "=", 2);	    
	    setenv(name_value_pair[0], name_value_pair[1], FALSE);
	    g_strfreev(name_value_pair);
	}
	
	execvp(cmd, args);
	_exit(1);
    }
    
    if(status == -1)
    {
	g_printerr("Error forking activation process!\n");
	disnix_emit_failure_signal(object, pid);
    }
    else
    {
	wait(&status);
	
	if(WEXITSTATUS(status) == 0)
	    disnix_emit_finish_signal(object, pid);
	else
	    disnix_emit_failure_signal(object, pid);
    }
    
    /* Free variables */
    g_free(arguments_string);
    
    _exit(0);
}

gboolean disnix_activate(DisnixObject *object, const gint pid, gchar *derivation, const gchar *type, gchar **arguments, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);

    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_activate_thread_func(object, pid, derivation, type, arguments);
    
    return TRUE;    
}

/* Deactivate method */

static void disnix_deactivate_thread_func(DisnixObject *object, const gint pid, gchar *derivation, const gchar *type, gchar **arguments)
{
    /* Declarations */
    gchar *arguments_string;
    int status;
        
    /* Generate arguments string */
    arguments_string = generate_derivations_string(arguments, " ");
    
    /* Print log entry */
    g_print("Deactivate: %s of type: %s with arguments: %s\n", derivation, type, arguments_string);
    
    /* Execute command */

    status = fork();
    
    if(status == 0)
    {
	unsigned int i;
	gchar *cmd = g_strconcat(activation_modules_dir, "/", type, NULL);
	char *args[] = {cmd, "deactivate", derivation, NULL};
	
	for(i = 0; i < g_strv_length(arguments); i++)
	{
	    gchar **name_value_pair = g_strsplit(arguments[i], "=", 2);	    
	    setenv(name_value_pair[0], name_value_pair[1], FALSE);
	    g_strfreev(name_value_pair);
	}
	
	execvp(cmd, args);
	_exit(1);
    }
    
    if(status == -1)
    {
	g_printerr("Error forking deactivation process!\n");
	disnix_emit_failure_signal(object, pid);
    }
    else
    {
	wait(&status);
	
	if(WEXITSTATUS(status) == 0)
	    disnix_emit_finish_signal(object, pid);
	else
	    disnix_emit_failure_signal(object, pid);
    }
    
    /* Free variables */
    g_free(arguments_string);
    
    _exit(0);
}

gboolean disnix_deactivate(DisnixObject *object, const gint pid, gchar *derivation, const gchar *type, gchar **arguments, GError **error)
{
    /* State object should not be NULL */
    g_assert(object != NULL);

    /* Fork job process which returns a signal later */
    if(fork() == 0)
	disnix_deactivate_thread_func(object, pid, derivation, type, arguments);
    
    return TRUE;    
}

gboolean disnix_lock(DisnixObject *object, const gint pid, GError **error)
{
    return TRUE;
}

gboolean disnix_unlock(DisnixObject *object, const gint pid, GError **error)
{
    return TRUE;
}

/* Get job id method */

gboolean disnix_get_job_id(DisnixObject *object, gint *pid, GError **error)
{
    g_printerr("Assigned job id: %d\n", job_counter);
    *pid = job_counter;
    job_counter++;
    return TRUE;
}
