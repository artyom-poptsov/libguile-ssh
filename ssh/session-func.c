/* session-func.c -- Functions for working with SSH session.
 *
 * Copyright (C) 2013 Artyom V. Poptsov <poptsov.artyom@gmail.com>
 *
 * This file is part of Guile-SSH.
 *
 * Guile-SSH is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * Guile-SSH is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Guile-SSH.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libguile.h>
#include <libssh/libssh.h>

#include "common.h"
#include "error.h"
#include "session-type.h"

/* SSH option mapping. */
struct option {
  char* symbol;
  int   type;
};


/* SSH options mapping to Guile symbols. */

static struct symbol_mapping session_options[] = {
  { "host",               SSH_OPTIONS_HOST               },
  { "port",               SSH_OPTIONS_PORT               },
  { "fd",                 SSH_OPTIONS_FD                 },
  { "bindaddr",           SSH_OPTIONS_BINDADDR           },
  { "user",               SSH_OPTIONS_USER               },
  { "ssh-dir",            SSH_OPTIONS_SSH_DIR            },
  { "identity",           SSH_OPTIONS_IDENTITY           },
  { "knownhosts",         SSH_OPTIONS_KNOWNHOSTS         },
  { "timeout",            SSH_OPTIONS_TIMEOUT            },
  { "timeout-usec",       SSH_OPTIONS_TIMEOUT_USEC       },
  { "ssh1",               SSH_OPTIONS_SSH1               },
  { "ssh2",               SSH_OPTIONS_SSH2               },
  { "log-verbosity",      SSH_OPTIONS_LOG_VERBOSITY      },
  { "ciphers-c-s",        SSH_OPTIONS_CIPHERS_C_S        },
  { "ciphers-s-c",        SSH_OPTIONS_CIPHERS_S_C        },
  { "compression-c-s",    SSH_OPTIONS_COMPRESSION_C_S    },
  { "compression-s-c",    SSH_OPTIONS_COMPRESSION_S_C    },
  { "proxycommand",       SSH_OPTIONS_PROXYCOMMAND       },
  { "stricthostkeycheck", SSH_OPTIONS_STRICTHOSTKEYCHECK },
  { "compression",        SSH_OPTIONS_COMPRESSION        },
  { "compression-level",  SSH_OPTIONS_COMPRESSION_LEVEL  },
  { NULL,                 -1 }
};

/* Blocking flush of the outgoing buffer.

   Return on of the following symbols: 'ok, 'error. 'again. */
SCM_DEFINE (guile_ssh_blocking_flush, "blocking-flush!", 2, 0, 0,
            (SCM session_smob, SCM timeout),
            "Blocking flush of the outgoing buffer.\n"
            "Return on of the following symbols: 'ok, 'error, 'again.")
#define FUNC_NAME s_guile_ssh_blocking_flush
{
  struct session_data *data = _scm_to_ssh_session (session_smob);

  int c_timeout;                /* Timeout */
  int res;                      /* Result of a function call. */

  /* Check types */
  SCM_ASSERT (scm_is_integer (timeout), timeout, SCM_ARG2, FUNC_NAME);

  c_timeout = scm_to_int (timeout);

  res = ssh_blocking_flush (data->ssh_session, c_timeout);
  switch (res)
    {
    case SSH_OK:
      return scm_from_locale_symbol ("ok");

    case SSH_AGAIN:
      return scm_from_locale_symbol ("again");

    case SSH_ERROR:
    default:
      return scm_from_locale_symbol ("error");
    }
}
#undef FUNC_NAME


/* Set SSH session options */

/* Convert VALUE to a string and pass it to ssh_options_set */
static inline int
set_string_opt (ssh_session session, int type, SCM value)
{
  char *str;
  int ret;

  SCM_ASSERT (scm_is_string (value),  value, SCM_ARG3, "session-set!");

  str = scm_to_locale_string (value);
  ret = ssh_options_set (session, type, str);
  free (str);

  return ret;
}

/* Convert VALUE to uint64 and pass it to ssh_options_set */
static inline int
set_uint64_opt (ssh_session session, int type, SCM value)
{
  uint64_t c_value;

  SCM_ASSERT (scm_is_unsigned_integer (value, 0, UINT64_MAX), value,
              SCM_ARG3, "session-set!");

  c_value = scm_to_uint64 (value);
  return ssh_options_set (session, type, &c_value);
}

/* Convert VALUE to uint32 and pass it to ssh_options_set */
static inline int
set_uint32_opt (ssh_session session, int type, SCM value)
{
  unsigned int c_value;

  SCM_ASSERT (scm_is_unsigned_integer (value, 0, UINT32_MAX), value,
              SCM_ARG3, "session-set!");

  c_value = scm_to_uint32 (value);
  return ssh_options_set (session, type, &c_value);
}

/* Convert VALUE to int32 and pass it to ssh_options_set */
static inline int
set_int32_opt (ssh_session session, int type, SCM value)
{
  int32_t c_value;

  SCM_ASSERT (scm_is_integer (value), value, SCM_ARG3, "session-set!");

  c_value = scm_to_int (value);
  return ssh_options_set (session, type, &c_value);
}

/* Convert VALUE to integer that represents a boolan value (0
   considered as false, any other value is true), and pass it to
   ssh_options_set */
static inline int
set_bool_opt (ssh_session session, int type, SCM value)
{
  int32_t bool;

  SCM_ASSERT (scm_is_bool (value), value, SCM_ARG3, "session-set!");

  bool = scm_to_bool (value);
  return ssh_options_set (session, type, &bool);
}

/* Convert VALUE to a socket file descriptor and pass it to
   ssh_options_set */
static inline int
set_port_opt (ssh_session session, int type, SCM value)
{
  socket_t sfd;                 /* Socket File Descriptor */

  SCM_ASSERT (scm_port_p (value), value, SCM_ARG3, "session-set!");

  sfd = scm_to_int (scm_fileno (value));

  return ssh_options_set (session, type, &sfd);
}

/* Convert Scheme symbol to libssh constant and set the corresponding
   option to the value of the constant. */
static inline int
set_sym_opt (ssh_session session, int type, struct symbol_mapping *sm, SCM value)
{
  struct symbol_mapping *opt = _scm_to_ssh_const (sm, value);
  if (! opt)
    guile_ssh_error1 ("session-set!", "Wrong value", value);
  return ssh_options_set (session, type, &opt->value);
}

/* Set an SSH session option. */
static int
set_option (ssh_session session, int type, SCM value)
{
  switch (type)
    {
    case SSH_OPTIONS_PORT:
      return set_uint32_opt (session, type, value);

    case SSH_OPTIONS_HOST:
    case SSH_OPTIONS_BINDADDR:
    case SSH_OPTIONS_USER:
    case SSH_OPTIONS_COMPRESSION:
    case SSH_OPTIONS_SSH_DIR:
    case SSH_OPTIONS_KNOWNHOSTS:
    case SSH_OPTIONS_IDENTITY:
    case SSH_OPTIONS_CIPHERS_C_S:
    case SSH_OPTIONS_CIPHERS_S_C:
    case SSH_OPTIONS_COMPRESSION_C_S:
    case SSH_OPTIONS_COMPRESSION_S_C:
    case SSH_OPTIONS_PROXYCOMMAND:
      return set_string_opt (session, type, value);

    case SSH_OPTIONS_LOG_VERBOSITY:
      return set_sym_opt (session, type, log_verbosity, value);

    case SSH_OPTIONS_COMPRESSION_LEVEL:
      return set_int32_opt (session, type, value);

    case SSH_OPTIONS_TIMEOUT:
    case SSH_OPTIONS_TIMEOUT_USEC:
      return set_uint64_opt (session, type, value);

    case SSH_OPTIONS_SSH1:
    case SSH_OPTIONS_SSH2:
    case SSH_OPTIONS_STRICTHOSTKEYCHECK:
      return set_bool_opt (session, type, value);

    case SSH_OPTIONS_FD:
      return set_port_opt (session, type, value);

    default:
      guile_ssh_error1 ("session-set!",
                        "Operation is not supported yet: %a~%",
                        scm_from_int (type));
    }

  return -1;                    /* ERROR */
}

/* Set a SSH option.  Return #t on success, #f on error. */
SCM_DEFINE (guile_ssh_session_set, "session-set!", 3, 0, 0,
            (SCM session, SCM option, SCM value),
            "Set a SSH option OPTION.  Throw an guile-ssh-error on error.\n"
            "Return value is undefined.")
#define FUNC_NAME s_guile_ssh_session_set
{
  struct session_data* data = _scm_to_ssh_session (session);
  struct symbol_mapping *opt;           /* Session option */
  int res;                              /* Result of a function call */

  SCM_ASSERT (scm_is_symbol (option), option, SCM_ARG2, FUNC_NAME);

  opt = _scm_to_ssh_const (session_options, option);

  if(! opt)
    guile_ssh_error1 (FUNC_NAME, "No such option", option);

  res = set_option (data->ssh_session, opt->value, value);
  if (res != SSH_OK)
    guile_ssh_error1 (FUNC_NAME, "Unable to set the option", option);

  scm_remember_upto_here_1 (session);

  return SCM_UNDEFINED;
}
#undef FUNC_NAME

/* Connect to the SSH server. 

   Return one of the following symbols: 'ok, 'again */
SCM_DEFINE (guile_ssh_connect_x, "connect!", 1, 0, 0,
            (SCM session),
            "Connect to the SSH server.\n"
            "Return one of the following symbols: 'ok, 'again")
#define FUNC_NAME s_guile_ssh_connect_x
{
  struct session_data* data = _scm_to_ssh_session (session);
  int res = ssh_connect (data->ssh_session);
  switch (res)
    {
    case SSH_OK:
      return scm_from_locale_symbol ("ok");

    case SSH_AGAIN:
      return scm_from_locale_symbol ("again");

    case SSH_ERROR:
    default:
      guile_ssh_error1 (FUNC_NAME, ssh_get_error (data->ssh_session),
                        session);
      return SCM_BOOL_F;        /* Not reached. */
    }
}
#undef FUNC_NAME

/* Disconnect from a session (client or server). 
   Return value is undefined.*/
SCM_DEFINE (guile_ssh_disconnect, "disconnect!", 1, 0, 0,
            (SCM arg1),
            "Disconnect from a session (client or server).\n"
            "Return value is undefined.")
{
  struct session_data* session_data = _scm_to_ssh_session (arg1);
  ssh_disconnect (session_data->ssh_session);
  return SCM_UNDEFINED;
}

/* Get SSH version.
 *
 * Return 1 for SSH1, 2 for SSH2 or #f on error
 */
SCM_DEFINE (guile_ssh_get_protocol_version, "get-protocol-version", 1, 0, 0,
            (SCM arg1),
            "Get SSH version.\n"
            "Return 1 for SSH1, 2 for SSH2 or #f on error.")
{
  struct session_data* data = _scm_to_ssh_session (arg1);
  SCM ret;
  int version = ssh_get_version (data->ssh_session);

  if (version >= 0)
    ret = scm_from_int (version);
  else
    ret = SCM_BOOL_F;

  return ret;
}

SCM_DEFINE (guile_ssh_get_error, "get-error", 1, 0, 1,
            (SCM arg1),
            "Retrieve the error text message from the last error.")
{
  struct session_data* data = _scm_to_ssh_session (arg1);
  SCM error = scm_from_locale_string (ssh_get_error (data->ssh_session));
  return error;
}

/* Authenticate the server.  

   Return one of the following symbols: 'ok, 'known-changed,
   'found-other, 'not-known, 'file-not-found */
SCM_DEFINE (guile_ssh_authenticate_server, "authenticate-server", 1, 0, 0,
            (SCM session),
            "Authenticate the server.\n"
            "Return one of the following symbols: 'ok, 'known-changed,\n"
            "'found-other, 'not-known, 'file-not-found")
#define FUNC_NAME s_guile_ssh_authenticate_server
{
  struct session_data* data = _scm_to_ssh_session (session);
  int res = ssh_is_server_known (data->ssh_session);

  switch (res)
    {
    case SSH_SERVER_KNOWN_OK:
      return scm_from_locale_symbol ("ok");

    case SSH_SERVER_KNOWN_CHANGED:
      return scm_from_locale_symbol ("known-changed");

    case SSH_SERVER_FOUND_OTHER:
      return scm_from_locale_symbol ("found-other");

    case SSH_SERVER_NOT_KNOWN:
      return scm_from_locale_symbol ("not-known");

    case SSH_SERVER_FILE_NOT_FOUND:
      return scm_from_locale_symbol ("file-not-found");

    case SSH_SERVER_ERROR:
    default:
      guile_ssh_error1 (FUNC_NAME, ssh_get_error (data->ssh_session),
                        session);
      return SCM_BOOL_F;        /* Not reached. */
    }
}
#undef FUNC_NAME

SCM_DEFINE (guile_ssh_get_public_key_hash, "get-public-key-hash", 1, 0, 0,
            (SCM session),
            "Get MD5 hash of the public key as a bytevector.\n"
            "Return a bytevector on success, #f on error.")
{
  struct session_data *session_data = _scm_to_ssh_session (session);
  unsigned char *hash = NULL;
  int hash_len;
  SCM ret;

  scm_dynwind_begin (0);

  hash_len = ssh_get_pubkey_hash (session_data->ssh_session, &hash);
  scm_dynwind_free (hash);

  if (hash_len >= 0)
    {
      size_t idx;
      ret = scm_c_make_bytevector (hash_len);
      for (idx = 0; idx < hash_len; ++idx)
        scm_c_bytevector_set_x (ret, idx, hash[idx]);
    }
  else
    {
      ret = SCM_BOOL_F;
    }

  scm_dynwind_end ();
  return ret;
}

/* Write the current server as known in the known hosts file.

   Return value is undefined. */
SCM_DEFINE (guile_ssh_write_known_host, "write-known-host!", 1, 0, 0,
            (SCM session),
            "Write the current server as known in the known hosts file.\n"
            "Return value is undefined.")
#define FUNC_NAME s_guile_ssh_write_known_host
{
  struct session_data *session_data = _scm_to_ssh_session (session);
  int res = ssh_write_knownhost (session_data->ssh_session);
  if (res != SSH_OK)
    {
      guile_ssh_error1 (FUNC_NAME, ssh_get_error (session_data->ssh_session),
                        session);
    }
                      
  return SCM_UNDEFINED;
}
#undef FUNC_NAME


/* Predicates */

/* Check if we are connected. 

   Return #f if we are connected to a server, #f if we aren't. */
SCM_DEFINE (guile_ssh_is_connected_p, "connected?", 1, 0, 0,
            (SCM arg1),
            "Check if we are connected.\n"
            "Return #f if we are connected to a server, #f if we aren't.")

{
  struct session_data* data = _scm_to_ssh_session (arg1);
  int res = ssh_is_connected (data->ssh_session);
  return scm_from_bool (res);
}


/* Initialize session related functions. */
void
init_session_func (void)
{
#include "session-func.x"
}

/* session-func.c ends here */