/*
           *** cmdftp, command line ftp client ***

    Copyright (C) 2003-2006 Claudio Fontana

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program (look for the file called COPYING);
    if not, write to the Free Software Foundation, Inc.,
        51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    You can contact the author (Claudio Fontana) by sending a mail
    to sick_soul@users.sourceforge.net
	                                                                 */

#ifndef CMDFTP_H
#define CMDFTP_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "auto-includes.h"

#ifndef AF_INET
#define AF_INET PF_INET
#endif

#if !HAVE_FSEEKO
# define fseeko fseek
# define ftello ftell
#endif

#if (SIZEOF_OFF_T == SIZEOF_INT)
# define OFF_FMT "%d"
#elif (SIZEOF_OFF_T == SIZEOF_LONG)
# define OFF_FMT "%ld"
#elif (SIZEOF_OFF_T == SIZEOF_LONG_LONG)
# define OFF_FMT "%lld"
#else
# define OFF_FMT "%ld"		/* should not happen, but provide a default */
#endif

#if !HAVE_STRCHR
# define strchr index
# define strrchr rindex
#endif

#if !HAVE_MEMCPY
# define memcpy(d, s, n) bcopy((s), (d), (n))
# define memset(s, c, n) bzero((s), (n))
#endif

#if !HAVE_SNPRINTF
# define snprintf(str, size, format, ...) sprintf(str, format, __VA_ARGS__)
#endif

#if !HAVE_STRDUP
char* strdup(char* s);
#endif

#if !HAVE_MKSTEMP
int mkstemp(char* template);
#endif

#define CMD_BUF_SIZE 2047
#define WIDE_LINE_LEN 160

enum { CMDFTP_LOCAL, CMDFTP_REMOTE };

enum { MODE_CHDIR, MODE_MKDIR, MODE_RMDIR, MODE_GETCWD,
       MODE_COPY, MODE_MOVE, MODE_UNLINK,
       MODE_FILE, MODE_SIZE,
       MODE_FETCH_LIST, MODE_FETCH_PRETTY_LIST,
       MODE_PRINT, MODE_EDIT
};

enum { CMDFTP_ERR_NULL, CMDFTP_ERR_SERV, CMDFTP_ERR_OPT,
       CMDFTP_ERR_CONN, CMDFTP_ERR_LGIN, CMDFTP_ERR_UNER,
       CMDFTP_ERR_HEAP, CMDFTP_ERR_PWD, CMDFTP_ERR_INTR,
       CMDFTP_ERR_TMPD
};

enum { CMDFTP_WAR_BYE,
       CMDFTP_WAR_OPEN, CMDFTP_WAR_WRIT, CMDFTP_WAR_READ, CMDFTP_WAR_NOCT,
       CMDFTP_WAR_MASK, CMDFTP_WAR_TEMP, CMDFTP_WAR_BADC, CMDFTP_WAR_MARG,
       CMDFTP_WAR_IARG,
       CMDFTP_WAR_LGIN, CMDFTP_WAR_FAIL, CMDFTP_WAR_MENV, CMDFTP_WAR_LOST,
       CMDFTP_WAR_INTR, CMDFTP_WAR_REST,
       CMDFTP_WAR_SRC, CMDFTP_WAR_TRG, CMDFTP_WAR_SKIP,
       CMDFTP_WAR_LERR, CMDFTP_WAR_RERR
};

enum { CMDFTP_ENV_PAGER, CMDFTP_ENV_EDITOR, CMDFTP_ENV_HOME, CMDFTP_ENV_TMPDIR,
       N_ENV };

enum { TRAN_INTR_NO, TRAN_INTR_INT, TRAN_INTR_PIPE };

enum { FILE_NEXIST, FILE_ISREG, FILE_ISDIR, FILE_OTHER };

typedef int (*fun_ptr)();	/* used as generic function pointer;
				   most return int but not all.*/

struct cmdftp_options {
  char* hostname; unsigned short p, b, a, q, m, n, g, d, P;
};

struct line_data {
  int count;
  char** lines;
};

struct list_entry {
  char* escaped_fullname;
  char* fullname;
  char* basename;
  char* dirname;
};

struct list_data {
  int count;
  struct list_entry* data;
};

void cmdftp_err(int code, char* msg);
void cmdftp_war(int code, char* msg);
void usage(void);
void version(void);
void intro(void);
void print_progress(char* op, char* fn, size_t fn_len, off_t cur_pos, off_t total_size);
void print_prompt(void);

void cleanexit(void);

void getoptions(int argc, char* argv[]);
void greeting(void);
void login_procedure(void);
int login(char* user, char* pass);
int manual_login(void);
int auto_login(void);
int auto_login_next_token(char** hay, char* store);
void read_token(char** hay, char* store);
int dispatch(char* cmd);
int local_chdir(char* arg);
int local_mkdir(char* arg);
int local_rmdir(char* arg);
char* local_getcwd(void);
int local_copy(char* target, char* source);
int local_move(char* old, char* new);
int local_unlink(char* name);
int local_file(char* name);
int local_fetch_list(char* filemask, struct list_data* d);
int local_fetch_pretty_list(struct list_data* d);
off_t local_size(FILE* f);
int local_print(char* arg);
int local_edit(char* arg);
int remote_chdir(char* arg);
int remote_chdir_aux(char* arg, char suppress_err);
int remote_mkdir(char* arg);
int remote_rmdir(char* arg);
char* remote_getcwd(void);
int remote_copy(char* target, char* source);
int remote_move(char* old, char* new);
int remote_unlink(char* arg);
int remote_file(char* name);
int remote_fetch_list(char* filemask, struct list_data* d);
int remote_fetch_pretty_list(struct list_data* d);
int remote_fetch_list_aux(char* filemask, struct list_data* d, char pretty);
off_t remote_size(char* filename);
int remote_print(char* arg);
int remote_edit(char* arg);

int do_home(int mode);
int do_copy_dir(char* target_dir, char* source_mask);
int do_move_dir(char* target_dir, char* source_mask);
int do_cp_mv_aux(char* arg, fun_ptr tx_file, fun_ptr tx_dir);
int do_ren(char* mask, char* from, char* to);
void do_setcwd(int mode);
int cmd_quit(char** argv); 
int cmd_h(char** argv); 
int cmd_l(char** argv);
int cmd_r(char** argv);
int cmd_pwd(char** argv);
int cmd_cd(char** argv);
int cmd_cp(char** argv);
int cmd_mv(char** argv);
int cmd_ren(char** argv);
int cmd_rm(char** argv);
int cmd_ls(char** argv);
int cmd_dir(char** argv);
int cmd_md(char** argv);
int cmd_rd(char** argv); 
int cmd_p(char** argv);
int cmd_e(char** argv);
int cmd_u(char** argv);
int u_aux(char* target_dir, char* source_mask);
int upload(char* target, FILE* source);
int cmd_d(char** argv);
int cmd_dr(char** argv);
int do_d(char** argv, int resume);
int d_aux(char* target_dir, char* source_mask, int resume);
int download(FILE* target, char* source);
int ls(struct list_data* d);
int cmdftp_connect(int port);
void cmdftp_reconnect(void);
ssize_t my_raw_read(char* buf, size_t n, int sc); 
ssize_t my_raw_write(char* buf, size_t n, int sc);
int send_command(char* cmd, char suppress_err); 
void reset_cmd_buffer(void);
int recv_confirm(void);
int recv_answer(int store, struct line_data* d, char suppress_err);
int getport();
void split_cmd(char* cmd, char** argv);
void free_cmd(char** argv);
int str_binsearch(char* key);
void readline_bs(char** ptr, int enable_echo);
void readline_tab(void);
void cmdftp_pwd_start(void);
void cmdftp_pwd_end(void);
void cmdftp_raw_mode(void);
void cmdftp_canon_mode(void);
int cmdftp_execute(char* p1, char* p2, int read_fd, int write_fd);

void canonized_fn(char* des[3], char* arg);
void free_fn(char* des[3]);
void init_signals(void);
void handler_INT(int i);
void init_lines(struct line_data* d);
void init_list(struct list_data* d);
void escape_list(struct list_data* d);
char* escape_string(char* filestring);
void store_line(char* line, struct line_data* d);
void store_list(char* fullname, struct list_data* d);
void store_pretty_list(char* fullname, struct list_data* d);
void free_lines(struct line_data* d);
void free_list(struct list_data* d);

char* local_getcwd(void);
char* remote_getcwd(void);
char* local_getcwd(void);
char* recv_line(int sc);
void* my_malloc(size_t s);
void* my_realloc(void* ptr, size_t s);

char* my_strdup(char* s);
char* readline(int enable_tab, int enable_echo);

int is_good_tmpdir(char* candidate);
void init_temp(void);
FILE* cmdftp_temp(char** fn);
char* clean_fn(char* fn);
char* fullpath(char* s, char* q);

#define TRANSFER_INTERRUPTED_CHECK(TRAN_INTR_CMD, TRAN_INTR_SYNC)  \
                                                                   \
 (transfer_interrupted == TRAN_INTR_INT) {                         \
  cmdftp_war(CMDFTP_WAR_INTR, TRAN_INTR_CMD);                      \
                                                                   \
  if (TRAN_INTR_SYNC) {                                            \
    if (recv_answer(0, 0, 1) == 150)                               \
      { close(cmdftp_data); recv_answer(0, 0, 1); }                \
  }                                                                \
  transfer_interrupted = TRAN_INTR_NO;                             \
                                                                   \
} else if (transfer_interrupted == TRAN_INTR_PIPE) {               \
  transfer_interrupted = TRAN_INTR_NO;                             \
  cmdftp_war(CMDFTP_WAR_LOST, TRAN_INTR_CMD);                      \
  cmdftp_reconnect();                                              \
  goto start_transfer;                                             \
} else if (0)

#endif /* CMDFTP_H */
