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
#include "cmdftp.h"

/*******************************/
/* ERROR, WARNING, ... STRINGS */
/*******************************/

char* msg_err[] = {
  0, "bad server response",

  "Usage: cmdftp [OPTION]... HOSTNAME\n"

  "Try '"
#if HAVE_GETOPT_LONG
  "cmdftp --help"
#else
  "cmdftp -h"
#endif
  "' for more information.",

  "connection failed", "giving up on login", "unexpected error", "heap allocation failed", "lost working directory", "double SIGINT, aborting", "no usable tmpdir"
};

char* msg_war[] = {
  "bye", "cannot open:", "error writing to:", "error reading from:", "transfer not confirmed", "file not found:", "cannot open temp file:", "unknown command:", "missing argument", "ignoring argument", "login failed", "operation failed or incomplete", "missing environment variable:", "resuming connection lost during", "interrupted during", "could not resume", "invalid source:", "invalid target:", "skipping:", "(local)", "(remote)"
};

#if HAVE_GETOPT_LONG

char* msg_usage = "cmdftp [OPTION]... HOSTNAME\n\n"
"options: \n"
" -h, --help       show this help and quit\n"
" -v, --version    show version and quit\n"
" -p, --port N     specify the remote TCP port of the server (def:21)\n"
" -b, --buffer N   specify the size in bytes of the transfer buffer (def:8192)\n"
" -a, --attempts N number of manual login attempts\n"
" -q, --quiet      be (very) quiet\n"
" -n, --no-auto    disable autologin (~/.netrc)\n"
" -m, --no-manual  disable manual login\n"
" -g, --no-paging  disable output paging\n"
" -d, --dotfiles   do not ignore dot-files in transfers\n"
" -P, --no-path    disable path in prompt\n";

#else
char* msg_usage = "cmdftp [OPTION]... HOSTNAME\n\n"
"options: \n"
" -h             show this help and quit\n"
" -v             show version and quit\n"
" -p N           specify the remote TCP port of the server (def:21)\n"
" -b N           specify the size in bytes of the transfer buffer (def:8192)\n"
" -a N           number of manual login attempts\n"
" -q             be (very) quiet\n"
" -n             disable autologin (~/.netrc)\n"
" -m             disable manual login\n"
" -g             disable output paging\n"
" -d             do not ignore dot-files in transfers\n"
" -P             disable path in prompt\n";
#endif

char* msg_version = PACKAGE_STRING " Copyright (c) 2003-2006 Claudio Fontana";

char* msg_intro = 
"cmdftp comes with ABSOLUTELY NO WARRANTY; this is free software, and you\n"
"are welcome to redistribute it under certain conditions specified by the\n"
"GPL (GNU General Public License). Look at the COPYING file for the details.\n"
"\nType h after the login to get help about internal commands.\n";

char* msg_help[] = {
  "h", "display this help",
  "l", "switch to local mode, following commands refer to local",
  "r", "switch to remote mode, following commands refer to remote",
  "pwd", "prompt working directory",
  "cd  PATH", "change working directory to PATH",
  "md  PATH", "make new directory PATH",
  "rd  PATH", "remove empty directory PATH",
  "rm  MASK", "delete regular files matching MASK, skip directories",
  "ls", "list current directory contents",
  "ls  MASK", "list files/dirs matching MASK",
  "dir", "pretty list of current directory",
  "cp  SRC TRG", "copy SRC to TRG. Behaves like /bin/cp -r",
  "mv  SRC TRG", "move SRC to TRG. Behaves like /bin/mv",
  "u   MASK DIR", "upload files/dirs matching MASK into remote DIR (recurs)",
  "d   MASK DIR", "download files/dirs matching MASK into local DIR (recurs)",
  "dr  MASK DIR", "same as above. If local file already exists, resume",
  "ren MASK FR TO", "replace FR to TO once in all filenames matching MASK",
  "p   FILE", "print contents of the FILE on the terminal",
  "e   FILE", "edit FILE",
  "q", "quit client",
  "quit|exit|bye", "aliases for q command",
  "<TAB>", "tab-completion"
};


/***********/
/* GLOBALS */
/***********/

struct cmdftp_options o = { 0, 21, 8192, 1, 0, 0, 0, 0, 0, 0 };
                                                      /* program options */

int cmdftp_control = 0;                               /* control connection */
int cmdftp_data = 0;                                  /* data connection */

char* user = 0; char* pass = 0;
char logging_in = 0;                                  /* user_logging_in? */
volatile int transfer_interrupted = TRAN_INTR_NO;     /* Why? */

struct hostent* server = 0;                           /* the remote host */
struct termios cmdftp_termios;                        /* terminal data */

char* buffer;                                         /* in/out file transf */

char cmd_buffer[CMD_BUF_SIZE + 1];                    /* in/out commands */
char cmd_line[CMD_BUF_SIZE + 1];                      /* in     line sep */
char cmd_userinput[CMD_BUF_SIZE + 1];                 /* buf for term in */
char* cmd_ptr;                                        /* ->cmd_buffer */

char* env[N_ENV];                                     /* used env vars */
char* cwd[2] = { 0 };                                 /* working dir {l, r} */

int mode = CMDFTP_REMOTE;                             /* current mode {l, r} */

struct sigaction sa;
char localhost[256] = { 0 };                          /* local host name */

/*********************************************/
/* USER COMMANDS and corresponding functions */
/*********************************************/

char* commands[] = { "bye", "cat", "cd", "cp", "d", "dir", "dr", "e", "exit", "h", "l", "ls", "md", "mkdir", "mv", "p", "pwd", "q", "quit", "r", "rd", "ren", "rename", "rm", "rmdir", "u" };

#define N_COMMANDS (sizeof(commands) / sizeof(char*))

fun_ptr cmd_functions[N_COMMANDS] = 
  { cmd_quit, cmd_p, cmd_cd, cmd_cp, cmd_d, cmd_dir, cmd_dr, cmd_e, cmd_quit, cmd_h, cmd_l, cmd_ls, cmd_md, cmd_md, cmd_mv, cmd_p, cmd_pwd, cmd_quit, cmd_quit, cmd_r, cmd_rd, cmd_ren, cmd_ren, cmd_rm, cmd_rd, cmd_u };

fun_ptr mf[2][13] = {
  { &local_chdir, &local_mkdir, &local_rmdir, (fun_ptr)local_getcwd, 
    &local_copy, &local_move, &local_unlink,
    &local_file, (fun_ptr)local_size,
    &local_fetch_list, &local_fetch_pretty_list,
    &local_print, &local_edit },

  { &remote_chdir, &remote_mkdir, &remote_rmdir, (fun_ptr)remote_getcwd,
    &remote_copy, &remote_move, &remote_unlink,
    &remote_file, (fun_ptr)remote_size,
    &remote_fetch_list, &remote_fetch_pretty_list,
    &remote_print, &remote_edit }
};

/************************************/
/* ERROR, WARNING, OUTPUT FUNCTIONS */
/************************************/


void cmdftp_err(int code, char* msg)
  { fprintf(stderr, "cmdftp: %s %s\n", msg_err[code], msg); exit(code); }

void cmdftp_war(int code, char* msg)
  { if (!o.q) fprintf(stderr, "cmdftp: %s %s\n", msg_war[code], msg); }

void usage(void) { printf("\n%s\n", msg_usage); }
void version(void) { printf("%s\n", msg_version); }

void intro(void) {
  int i; if (o.q) return;
  for (i = 0; i < 79; i++) fputc('*', stdout);
  fputc('\n', stdout);
  version();
  printf("\n%s\n", msg_intro);
  for (i = 0; i < 79; i++) fputc('*', stdout);
  fputc('\n', stdout);
}

void print_progress(char* op, char* fn, size_t fn_len, off_t cur_pos, off_t total_size) {

  int abbrev;
  char* units[] = { "", "K", "M", "G", "T" }; int u_idx;
  char line[WIDE_LINE_LEN]; size_t line_len;
  double progress = 
    total_size > 0 ? (cur_pos * 100.0 / total_size) : 100.0;

  u_idx = 0;

  while ((total_size >> 10) >= 10 && u_idx < 4) { 
    total_size >>= 10; cur_pos >>= 10; u_idx++;
  }

  snprintf(line, 75, "%s " OFF_FMT "%s/" OFF_FMT "%s (%4.*f%%) ",
	   op, cur_pos, units[u_idx], total_size, units[u_idx],
	   progress < 1.0 ? 2 : 1, progress);

  line[74] = '\0';		/* just to be sure that*/
  line_len = strlen(line);	/* line_len <= 74 */

  if (line_len + fn_len < 79) {
    strcpy(line + line_len, fn);

  } else {
    sprintf(line + line_len, "[...]%s", (fn + fn_len - (74 - line_len)));
  }

  printf("%s\r", line);
  fflush(stdout);
}

/***************************************/
/* MAIN and other high level functions */
/***************************************/

void print_prompt(void) {
  if (!o.q)
    printf("\r%s:%s>", mode == CMDFTP_LOCAL ?
	   localhost : o.hostname, o.P ? "" : cwd[mode]);
  fflush(stdout);
}

int main(int argc, char* argv[]) {

  if (argc < 2) cmdftp_err(CMDFTP_ERR_OPT, "");
  getoptions(argc, argv);
  
  if (gethostname(localhost, sizeof(localhost)) < 0)
    strcpy(localhost, "local");

  intro();
  reset_cmd_buffer(); buffer = my_malloc(o.b);

  tcgetattr(0, &cmdftp_termios); atexit(&cmdftp_canon_mode);
  init_signals();
  init_temp();

  if (!(cmdftp_control = cmdftp_connect(o.p)))
    cmdftp_err(CMDFTP_ERR_CONN, "");

  greeting();
  login_procedure();

  do_setcwd(CMDFTP_LOCAL);
  do_setcwd(CMDFTP_REMOTE);

  while (1) {
    char* cmd;
    print_prompt();
    if (!dispatch(cmd = readline(1, 1))) return 0;
    free(cmd);
  }
}

#if HAVE_GETOPT_LONG

struct option long_options[] = { 
  { "help", 0, 0, 'h' },
  { "version", 0, 0, 'v' },
  { "attempts", 1, 0, 'a' },
  { "port", 1, 0, 'p' },
  { "buffer", 1, 0, 'b' },
  { "quiet", 1, 0, 'q' },
  { "no-manual", 0, 0, 'm' },
  { "no-auto", 0, 0, 'n' },
  { "no-paging", 0, 0, 'g' },
  { "no-path", 0, 0, 'P' },
  { "dotfiles", 0, 0, 'd' },
  { 0, 0, 0, 0 }
};

#endif

void getoptions(int argc, char* argv[]) {
  int i, rv;

  while ((i =

#if HAVE_GETOPT_LONG
	  getopt_long(argc, argv, "Pa:b:dghmnp:qv", long_options, 0)
#else
	  getopt(argc, argv, "Pa:b:dghmnp:qv")
#endif
	  
	  ) != -1) {
    rv = 1;
    
    switch (i) {
    case 'h' : usage(); cleanexit();
    case '?' : case ':' : cmdftp_err(CMDFTP_ERR_OPT, "");
    case 'v' : version(); cleanexit();

    case 'a' : rv = sscanf(optarg, "%hu", &o.a); break;
    case 'p' : rv = sscanf(optarg, "%hu", &o.p); break;
    case 'b' : rv = sscanf(optarg, "%hu", &o.b); break;

    case 'q' : o.q = 1; break;
    case 'm' : o.m = 1; break;
    case 'n' : o.n = 1; break;
    case 'g' : o.g = 1; break;
    case 'd' : o.d = 1; break;
    case 'P' : o.P = 1; break;
    }
    if (rv != 1) cmdftp_err(CMDFTP_ERR_OPT, "");
  }

  if (optind >= argc) cmdftp_err(CMDFTP_ERR_OPT, "");

  o.hostname = my_strdup(argv[optind]);
  
  env[CMDFTP_ENV_PAGER] = o.g ? 0 : getenv("PAGER");
  env[CMDFTP_ENV_EDITOR] = getenv("EDITOR");
  env[CMDFTP_ENV_HOME] = getenv("HOME");
  env[CMDFTP_ENV_TMPDIR] = getenv("TMPDIR");
}

void greeting(void) {
  if (recv_answer(0, 0, 0) != 220) cmdftp_err(CMDFTP_ERR_SERV, "(greeting)");
}


/*********************************/
/* LOGIN AND AUTOLOGIN FUNCTIONS */
/*********************************/

void login_procedure(void) {
  int attempt;
  user = pass = 0;

  if (!o.n) {
    if (auto_login()) return;
  }

  if (!o.m) {
    for (attempt = 0; attempt < o.a; attempt++) {
      if (user) free(user); if (pass) free(pass);
      user = pass = 0;
      if (manual_login()) return;
      cmdftp_war(CMDFTP_WAR_LGIN, "");
    }
  }

  cmdftp_err(CMDFTP_ERR_LGIN, "");
}

int login(char* user, char* pass) {
  int rv = 0;
  
  logging_in = 1;
  snprintf(cmd_buffer, CMD_BUF_SIZE, "USER %s", user);
  rv = send_command(cmd_buffer, 0);
  if (rv == 230) return 1;
  if (rv != 331) return 0;

  snprintf(cmd_buffer, CMD_BUF_SIZE, "PASS %s", pass);
  if (send_command(cmd_buffer, 0) == 230) rv = 1;
  logging_in = 0;
  return rv;
}

int manual_login(void) {
  printf("Username: ");
  if (!(user = readline(0, 1))) return 0;

  cmdftp_pwd_start(); printf("Password: ");
  pass = readline(0, 0);
  cmdftp_pwd_end();

  if (!pass) return 0;
  return login(user, pass);
}

int auto_login(void) {
  FILE* f; char* fname; char line[WIDE_LINE_LEN]; char token[WIDE_LINE_LEN];
  int state, rv;
  char* rc = ".netrc"; state = rv = 0;

  if (!env[CMDFTP_ENV_HOME]) return 0;
  f = fopen((fname = fullpath(env[CMDFTP_ENV_HOME], rc)), "r");
  free(fname);
  if (!f) return 0;

  while((fgets(line, WIDE_LINE_LEN, f))) {
    char* ptr, *ptr_new, *ptr_tmp; size_t l;
    l = strlen(line); if (line[l - 1] == '\n') line[l - 1] = 0;
    if ((ptr = strchr(line, '#')))
      if (ptr == line || ptr[-1] != '\\') *ptr = 0;
    ptr = ptr_new = ptr_tmp = line;

  tokenize:
    if ((l = auto_login_next_token(&ptr, token)) == -1) continue;
    if (l == 0) {
      if (!state)
	{ if (strcmp(o.hostname, token) == 0) state++; goto tokenize; }
      break;
    } else if (l == 1) {
      if (!state) { state++; goto tokenize; }
      break;
    } else if (l == 2) {
      if (state && !user) user = my_strdup(token); goto tokenize;
    } else if (l == 3) {
      if (state && !pass) pass = my_strdup(token); goto tokenize;
    }
  } 
  if (user && !pass) pass = my_strdup("");
  if (user) rv = login(user, pass);
  if (!rv) {
    if (user) { free(user); user = 0; }
    if (pass) { free(pass); pass = 0; }
  }
  return rv;
}

int auto_login_next_token(char** hay, char* store) {
#define AUTO_N_KEYS 4
  char* keys[] = { "machine", "default", "login", "password" };
  int keys_len[] = { 7, 7, 5, 8 };
  char* ptr, *minptr; int i, min;

  min = -1; minptr = 0;

  for (i = 0; i < AUTO_N_KEYS; i++)
    if ((ptr = strstr(*hay, keys[i])))
      if (!minptr || ptr < minptr) { minptr = ptr; min = i; }

  if (min != -1) {
    *hay = minptr + keys_len[min];
    if (min != 1) {
      if (!isspace(**hay)) {
	*store = 0;
      } else {
	while (isspace(**hay)) (*hay)++;
	read_token(hay, store);
      }
    }
  }
  return min;
#undef AUTO_N_KEYS
}

void read_token(char** hay, char* store) {
  int i, c;

  for (i = 0; ; i++) {
    c = **hay;
    if (!c || isspace(c)) break;

    if (c == '\\') {
      int n = 0; (*hay)++;
      if (!(c = **hay)) break;

      if (isdigit(c)) {
	sscanf(*hay, "%3o%n", (unsigned int*)&c, &n); *hay += n - 1;

      } else if (c == 'x') {
	(*hay)++; sscanf(*hay, "%2x%n", (unsigned int*)&c, &n); *hay += n - 1;

      } else {
	if (c == 'a') c = '\a'; else if (c == 'b') c = '\b';
	else if (c == 'f') c = '\f'; else if (c == 'n') c = '\n';
	else if (c == 't') c = '\t'; else if (c == 'v') c = '\v';
	else {
	/* get character as if no backslash present, good to escape spaces */
	}
      }
    }
    store[i] = c;
    (*hay)++;
  }

  store[i] = 0;
}


/***********************/
/* FUNCTION DISPATCHER */
/***********************/


int dispatch(char* cmd) {
  int idx;
  char* argv[4];

  if (!cmd) return 0;
  split_cmd(cmd, argv);

  if (*argv[0] == 0) return 1;		/* the empty, do nothing command */

  if ((idx = str_binsearch(argv[0])) == -1)
    cmdftp_war(CMDFTP_WAR_BADC, cmd);
  else if (!cmd_functions[idx](&argv[1]))
    cmdftp_war(CMDFTP_WAR_FAIL, "");

  free_cmd(argv);
  return 1;
}

/*************************************************/
/* LOCAL MODE SPECIFIC UTILITY FUNCTIONS: local_ */
/*************************************************/

int local_chdir(char* arg) {
  /* watch return value! */
  int rv;

  if ((rv = chdir(arg)) < 0)
    cmdftp_war(CMDFTP_WAR_LERR, strerror(errno));

  return rv;
}

int local_mkdir(char* arg) {
  int rv;

  if (!(rv = (mkdir(arg, 0777) == 0)))
    cmdftp_war(CMDFTP_WAR_LERR, strerror(errno));

  return rv;
}

int local_rmdir(char* arg) {
  int rv;

  if (!(rv = (rmdir(arg) == 0)))
    cmdftp_war(CMDFTP_WAR_LERR, strerror(errno));

  return rv;
}

char* local_getcwd(void) {
  return getcwd(cmd_buffer, CMD_BUF_SIZE);
}

int local_copy(char* target, char* source) {
  FILE* t, *s; off_t start_pos, total_size;
  char* csource; char* op;
  size_t csource_len;
  int rv = 0;
  
  if (!(s = fopen(source, "rb"))) return 0;

  if ((total_size = local_size(s)) >= 0 && (t = fopen(target, "wb"))) {

    op = "Copying"; 
    csource = clean_fn(source); csource_len = strlen(csource);

  start_transfer:
    start_pos = 0;

    if (!o.q)
      print_progress(op, csource, csource_len, start_pos, total_size);

    while (start_pos < total_size) {
      size_t toread, bytes; ssize_t written;

      toread = ((total_size - start_pos) < o.b) ? total_size - start_pos : o.b;
      
      if (!(bytes = fread(buffer, 1, toread, s))) break;
      if ((written = fwrite(buffer, 1, bytes, t)) != bytes) break;
      
      start_pos += bytes;

      if (!o.q)
	print_progress(op, csource, csource_len, start_pos, total_size);
    }

    if (!o.q) fputc('\n', stdout);

    if (start_pos < total_size) {
      if (ferror(s)) cmdftp_war(CMDFTP_WAR_READ, source);
      else if (ferror(t)) cmdftp_war(CMDFTP_WAR_WRIT, target);
      else if TRANSFER_INTERRUPTED_CHECK("local copy", 0);
      else { cmdftp_err(CMDFTP_ERR_UNER, strerror(errno)); }

    } else {
      rv = 1;
    }
    fclose(t);
  }
  fclose(s);
  return rv;
}

int local_move(char* new, char* old) {
  int rv;

  if (!(rv = (rename(old, new) == 0)))
    cmdftp_war(CMDFTP_WAR_LERR, strerror(errno));

  return rv;
}

int local_unlink(char* name) {
  int rv;

  if (!(rv = (unlink(name) == 0)))
    cmdftp_war(CMDFTP_WAR_LERR, strerror(errno));

  return rv;
}

int local_file(char* name) {
  struct stat buf;

  if (stat(name, &buf) != 0)
    return FILE_NEXIST;
  
  return (S_ISREG(buf.st_mode) ? FILE_ISREG : 
	  S_ISDIR(buf.st_mode) ? FILE_ISDIR :
	  FILE_OTHER);
}

int local_fetch_list(char* filemask, struct list_data* d) {
  char* fn_mask[3], *tmpmask;
  struct dirent* entry;
  DIR* dir;
  int rv = 0;
  
  if (*filemask == 0) tmpmask = my_strdup("*");
  else tmpmask = my_strdup(filemask);

  canonized_fn(fn_mask, tmpmask);

  if (!(dir = opendir(filemask ? fn_mask[0] : ".")))
    goto end_proc;
  
  while ((entry = readdir(dir))) {
    if (fnmatch(fn_mask[1], entry->d_name, 0) == 0) {
      char* full_name = fullpath(fn_mask[0], entry->d_name);
      store_list(clean_fn(full_name), d);
      free(full_name);
    }
  }

  closedir(dir); rv = 1;

 end_proc:
  free(tmpmask); free_fn(fn_mask);
  return rv;
}

int local_fetch_pretty_list(struct list_data* d) {
  int pipe_fd[2]; FILE* target; char line[CMD_BUF_SIZE];

  if (pipe(pipe_fd) < 0) return 0;

  if (!cmdftp_execute("/bin/ls -l", 0, -1, pipe_fd[1])) {
    close(pipe_fd[0]); close(pipe_fd[1]);
    return local_fetch_list("", d);
  }

  close(pipe_fd[1]);

  if (!(target = fdopen(pipe_fd[0], "r")))
    { close(pipe_fd[0]); return 0; }

  while (fgets(line, CMD_BUF_SIZE, target)) {
    size_t len = strlen(line);
    if (line[len - 1] == '\n') line[len - 1] = 0;
    store_pretty_list(line, d);
  }
  fclose(target); 
  return 1;
}

off_t local_size(FILE* f) {
  off_t rv;
  fseeko(f, 0, SEEK_END); rv = ftello(f); fseeko(f, 0, SEEK_SET);
  return rv;
}

int local_print(char* arg) {
  if (env[CMDFTP_ENV_PAGER])
    return cmdftp_execute(env[CMDFTP_ENV_PAGER], arg, -1, -1);
  else {
    FILE* f; int c;
    if (!(f = fopen(arg, "r"))) 
      { cmdftp_war(CMDFTP_WAR_OPEN, arg); return 0; }
    while ((c = getc(f)) != EOF) fputc(c, stdout); fputc('\n', stdout);
    fclose(f);
  }
  return 1;
}

int local_edit(char* arg) {
  return cmdftp_execute(env[CMDFTP_ENV_EDITOR], arg, -1, -1);
}

/***************************************************/
/* REMOTE MODE SPECIFIC UTILITY FUNCTIONS: remote_ */
/***************************************************/

int remote_chdir(char* arg) {
  return remote_chdir_aux(arg, 0);
}

int remote_chdir_aux(char* arg, char suppress_err) {
  /* watch the return value! */
  int rv;

  snprintf(cmd_buffer, CMD_BUF_SIZE, "CWD %s", arg);
  if (!(rv = send_command(cmd_buffer, suppress_err)))
    return -2;			/* bad err, intr */
  
  if (rv == 250) return 0;	/* ok */
  return -1;			/* fail */
}

int remote_mkdir(char* arg) {
  snprintf(cmd_buffer, CMD_BUF_SIZE, "MKD %s", arg);
  return (send_command(cmd_buffer, 0) == 257);
}

int remote_rmdir(char* arg) {
  snprintf(cmd_buffer, CMD_BUF_SIZE, "RMD %s", arg);
  return (send_command(cmd_buffer, 0) == 250);
}

char* remote_getcwd(void) {
  char* start, *end;
  if (!send_command("PWD", 1)) return 0;
  if ((end = strrchr(cmd_buffer, '"')) == 0 ||
      (start = strchr(cmd_buffer, '"')) == 0) return 0;
  *end = 0; start++;
  return start;
}

int remote_copy(char* target, char* source) {
  FILE* t; char* t_fn;
  int rv = 0;

  if (!(t = cmdftp_temp(&t_fn)))
    { cmdftp_war(CMDFTP_WAR_TEMP, t_fn); return 0; }

  if (download(t, source) && upload(target, t))
    rv = 1;

  fclose(t); unlink(t_fn); free(t_fn);
  return rv;
}

int remote_move(char* to, char* from) {
  snprintf(cmd_buffer, CMD_BUF_SIZE, "RNFR %s", from);
  if (send_command(cmd_buffer, 0) != 350) return 0;
 
  snprintf(cmd_buffer, CMD_BUF_SIZE, "RNTO %s", to);
  if (send_command(cmd_buffer, 0) != 250) return 0;

  return 1;
}

int remote_unlink(char* arg) {
  snprintf(cmd_buffer, CMD_BUF_SIZE, "DELE %s", arg);
  return (send_command(cmd_buffer, 0) == 250);
}

int remote_file(char* name) {
  off_t size; int rv;

  rv = remote_chdir_aux(name, 1);

  if (rv == 0) { /* ok */
    return do_home(CMDFTP_REMOTE) ? FILE_ISDIR : -1;
    
  } else if (rv == -2) { /* bad err, intr */
    do_home(CMDFTP_REMOTE);
    return -2;
    
  } else /* (rv == -1) */ if ((size = remote_size(name)) >= 0) {
    return FILE_ISREG;

  } else if (size == -1) {
    return FILE_NEXIST;
    
  } else /* (size == -2) */ {
    return -1;
  }
}

int remote_fetch_list(char* filemask, struct list_data* d) {
  return remote_fetch_list_aux(filemask, d, 0);
}

int remote_fetch_pretty_list(struct list_data* d) {
  return remote_fetch_list_aux(0, d, 1);
}

int remote_fetch_list_aux(char* filemask, struct list_data* d, char pretty) {
  char* tmpmask, *fn_mask[3], *line;
  int tmp, rv = 0;

#ifdef CMDFTP_LIST_HACK
  int hack = 0;
#endif

  int port;

  if (filemask) {
    if (*filemask == 0) tmpmask = my_strdup("*");
    else tmpmask = my_strdup(filemask);

    canonized_fn(fn_mask, tmpmask);
  }
  
 start_transfer:

  if (!send_command("TYPE A", 0)) goto end_proc;
 
  if (!(port = getport())) goto end_proc;
  if (!(cmdftp_data = cmdftp_connect(port))) goto end_proc;

#ifdef CMDFTP_LIST_HACK
  if (filemask) {
    hack = 1;
    if (remote_chdir(fn_mask[0]) != 0) goto end_proc;
  }

  sprintf(cmd_buffer, pretty ? "LIST" : "NLST");
#else
  if (filemask) {
    snprintf(cmd_buffer, CMD_BUF_SIZE, pretty ? "LIST %s" : "NLST %s",
	     fn_mask[0]);
  } else {
    sprintf(cmd_buffer, pretty ? "LIST" : "NLST");
  }
#endif /* CMDFTP_LIST_HACK */
  
  if ((tmp = (send_command(cmd_buffer, 1))) != 150)
    { close(cmdftp_data); if (tmp == 450) rv = 1; goto end_proc; }

  reset_cmd_buffer(); 

  transfer_interrupted = TRAN_INTR_NO;

  while ((line = recv_line(cmdftp_data))) {
    if (!pretty) {
      char* fn;
      if (!(fn = strrchr(line, '/'))) fn = line;
      else fn++;

      if (!filemask || fnmatch(fn_mask[1], fn, 0) == 0) {
	char* full_name = filemask ? fullpath(fn_mask[0], fn) : fn;
	store_list(clean_fn(full_name), d);
	if (filemask) free(full_name);
      }
    } else {
      store_pretty_list(line, d);
    }
  }

  close(cmdftp_data); 

  if TRANSFER_INTERRUPTED_CHECK("data transfer", 1);
  else {
    if (!recv_confirm()) cmdftp_war(CMDFTP_WAR_NOCT, "");
    else rv = 1;
  }
  
 end_proc:
  if (filemask) { free(tmpmask); free_fn(fn_mask); }

#ifdef CMDFTP_LIST_HACK
  if (hack) {
    rv = (remote_chdir(cwd[CMDFTP_REMOTE]) == 0) && rv;
  }
#endif
  
  return rv;
}  

off_t remote_size(char* filename) {
  off_t size; int rv;
  snprintf(cmd_buffer, CMD_BUF_SIZE, "SIZE %s", filename);
  if (!(rv = send_command(cmd_buffer, 1))) return -2;
  else if (rv != 213) return -1;
  
  sscanf(cmd_buffer + 4, OFF_FMT, &size);
  return size;
}

int remote_print(char* arg) {
  int c; char* fn; FILE* f; int rv = 0;

  if (!(f = cmdftp_temp(&fn))) { cmdftp_war(CMDFTP_WAR_TEMP, fn); return 0; }

  if (download(f, arg)) {
    if (env[CMDFTP_ENV_PAGER]) {
      fclose(f); f = 0;
      rv = cmdftp_execute(env[CMDFTP_ENV_PAGER], fn, -1, -1);
    } else {
      fseeko(f, 0, SEEK_SET);
      while ((c = getc(f)) != EOF) fputc(c, stdout); fputc('\n', stdout);
      rv = 1;
    }
  }

  if (f) fclose(f); unlink(fn); free(fn); return rv;
}

int remote_edit(char* arg) {
  char* fn; FILE* f; int rv = 0;

  if (!(f = cmdftp_temp(&fn))) { cmdftp_war(CMDFTP_WAR_TEMP, fn); return 0; }

  if (download(f, arg)) {
    fclose(f); f = 0;
    if (cmdftp_execute(env[CMDFTP_ENV_EDITOR], fn, -1, -1)) {
      if ((f = fopen(fn, "r")) && upload(arg, f)) rv = 1;
    }
  }
  if (f) fclose(f); unlink(fn); free(fn); return rv;
}


/***********************************/
/* COMPOSITE ACTIONS: do_          */
/***********************************/

int do_home(int mode) {
  /* home to current directory of mode */

  if (mf[mode][MODE_CHDIR](cwd[mode]) == 0)
    return 1;			/* ok */
  else if (mf[mode][MODE_CHDIR](cwd[mode]) == 0)
    return 0;			/* ok, but stop current op */
  else 
    cmdftp_err(CMDFTP_ERR_PWD, "");

  return 0;			/* never reached */
}

int do_copy_dir(char* target_dir, char* source_mask) {
  struct list_data d;
  int i, rv;

  if ((rv = mf[mode][MODE_FILE](target_dir)) == FILE_NEXIST) {
    if (!mf[mode][MODE_MKDIR](target_dir))
      return 0;

  } else if (rv != FILE_ISDIR) {
    return 0;
  }

  init_list(&d);

  if (!mf[mode][MODE_FETCH_LIST](source_mask, &d)) return 0;
  if (d.count == 0) return 1;	/* nothing to do but ok */
  
  rv = 1;
  for (i = 0; i < d.count; i++) {
    char* target; int st, tt;
    target = fullpath(target_dir, d.data[i].basename);
    
    if ((tt = mf[mode][MODE_FILE](target)) < 0 ||
	(st = mf[mode][MODE_FILE](d.data[i].fullname)) < 0) {
      rv = 0;
    } else if (tt == FILE_OTHER ||
	       (st == FILE_ISDIR && tt == FILE_ISREG) ||
	       (st == FILE_ISREG && tt == FILE_ISDIR) ||
	       (st != FILE_ISREG && st != FILE_ISDIR)) {
      
      cmdftp_war(CMDFTP_WAR_SKIP, d.data[i].fullname);

    } else if (st == FILE_ISREG) {
      if (!(rv = mf[mode][MODE_COPY](target, d.data[i].fullname)))
	rv = 0;

    } else /* (st == FILE_ISDIR) */ {
      char* new_source = fullpath(d.data[i].fullname, "*");
      if (!do_copy_dir(target, new_source)) rv = 0;
      free(new_source);
    }
    free(target);
    if (!rv) break;
  }
  free_list(&d);
  return rv;
}

int do_move_dir(char* target_dir, char* source_mask) {
  struct list_data d; 
  int i, rv;

  init_list(&d);

  if (!mf[mode][MODE_FETCH_LIST](source_mask, &d)) return 0;
  if (d.count == 0) return 1;	/* nothing to do but ok */

  rv = 1;
  for (i = 0; i < d.count; i++) {
    char* target; int st, tt;
    target = fullpath(target_dir, d.data[i].basename);
    
    if ((tt = mf[mode][MODE_FILE](target)) < 0 ||
	(st = mf[mode][MODE_FILE](d.data[i].fullname)) < 0) {
      rv = 0;

    } else if (tt == FILE_ISDIR ||
	       tt == FILE_OTHER ||
	       (st == FILE_ISDIR && tt == FILE_ISREG) ||
	       (st != FILE_ISREG && st != FILE_ISDIR)) {
      
      cmdftp_war(CMDFTP_WAR_SKIP, d.data[i].fullname);
      
    } else {
      if (!mf[mode][MODE_MOVE](target, d.data[i].fullname))
	rv = 0;
    }
    free(target);
    if (!rv) break;
  }
  free_list(&d);
  return rv;
}

int cmd_cp(char** paths) {
  struct list_data ld;
  char* source[3]; char* target[3];
  int source_type, target_type;
  int rv = 0;

  if (*paths[0] == 0 || *paths[1] == 0)
    { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }

  canonized_fn(source, paths[0]);
  canonized_fn(target, paths[1]);

  init_list(&ld);

  if (!mf[mode][MODE_FETCH_LIST](source[2], &ld)) goto end_proc;

  if (ld.count == 0)
    { cmdftp_war(CMDFTP_WAR_MASK, paths[0]); rv = 1; goto end_proc; }

  if ((target_type = mf[mode][MODE_FILE](target[2])) < 0) goto end_proc;
 
  if (ld.count == 1) {
    if ((source_type = mf[mode][MODE_FILE](ld.data[0].fullname)) < 0)
      goto end_proc;

    else if (source_type == FILE_ISREG) {
      /* source is a single regular file */
      
      if (target_type == FILE_NEXIST || target_type == FILE_ISREG) {
	rv = mf[mode][MODE_COPY](target[2], ld.data[0].fullname);
	
      } else if (target_type == FILE_ISDIR) {
	char* new_target = fullpath(target[2], ld.data[0].basename);
	free(target[2]); target[2] = new_target;
	rv = mf[mode][MODE_COPY](target[2], ld.data[0].fullname);

      } else {
	cmdftp_war(CMDFTP_WAR_TRG, "exists (not a regfile or a dir)");
      }
    } else if (source_type == FILE_ISDIR) {
      /* source is a single directory */
      if (target_type == FILE_ISDIR) {
	rv = do_copy_dir(target[2], ld.data[0].fullname);
      } else {
	char* new_source = fullpath(ld.data[0].fullname, "*");
	rv = do_copy_dir(target[2], new_source);
	free(new_source);
      }

    } else {
      cmdftp_war(CMDFTP_WAR_SRC, "not a regular file or a dir");
    }

  } else if (target_type == FILE_ISDIR) {
    /* source is multiple entries, target is an existing directory */
    rv = do_copy_dir(target[2], source[2]);

  } else {
    cmdftp_war(CMDFTP_WAR_TRG, "multiple source needs a dir target");
  }
  
 end_proc:
  free_list(&ld);
  free_fn(source); free_fn(target);
 
  return rv;
}

int cmd_mv(char** paths) {
  struct list_data ld;
  char* source[3]; char* target[3];
  int source_type, target_type;
  int rv = 0;

  if (*paths[0] == 0 || *paths[1] == 0)
    { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }

  canonized_fn(source, paths[0]);
  canonized_fn(target, paths[1]);

  init_list(&ld);

  if (!mf[mode][MODE_FETCH_LIST](source[2], &ld)) goto end_proc;

  if (ld.count == 0)
    { cmdftp_war(CMDFTP_WAR_MASK, paths[0]); rv = 1; goto end_proc; }

  if ((target_type = mf[mode][MODE_FILE](target[2])) < 0) goto end_proc;
  
  if (ld.count == 1) {
    if ((source_type = mf[mode][MODE_FILE](ld.data[0].fullname)) < 0)
      goto end_proc;

    else if (source_type == FILE_ISREG) {
      if (target_type == FILE_NEXIST || target_type == FILE_ISREG) {
	rv = mf[mode][MODE_MOVE](target[2], ld.data[0].fullname);

      } else if (target_type == FILE_ISDIR) {
	char* new_target = fullpath(target[2], ld.data[0].basename);
	free(target[2]); target[2] = new_target;
	rv = mf[mode][MODE_MOVE](target[2], ld.data[0].fullname);

      } else {
	cmdftp_war(CMDFTP_WAR_TRG, "exists (not a regfile or a dir)");
      }

    } else if (source_type == FILE_ISDIR) {
      if (target_type == FILE_NEXIST) {
	rv = mf[mode][MODE_MOVE](target[2], ld.data[0].fullname);      

      } else if (target_type == FILE_ISDIR) {
	rv = do_move_dir(target[2], ld.data[0].fullname);

      } else {
	cmdftp_war(CMDFTP_WAR_TRG, "exists (not a dir)");
      }

    } else {
      cmdftp_war(CMDFTP_WAR_SRC, "not a regular file or a dir");
    }

  } else if (target_type == FILE_ISDIR) {
    /* source is multiple entries, target is an existing directory */
    rv = do_move_dir(target[2], source[2]);

  } else {
    cmdftp_war(CMDFTP_WAR_TRG, "multiple source needs a dir target");
  }

 end_proc:
  free_list(&ld);
  free_fn(source); free_fn(target);

  return rv;
}


int do_ren(char* mask, char* from, char* to) {
  struct list_data ld;
  int i, rv;
  size_t len_from, len_to;

  len_from = strlen(from); len_to = strlen(to);

  init_list(&ld);

  if (!mf[mode][MODE_FETCH_LIST](mask, &ld)) return 0;
  if (ld.count == 0) { cmdftp_war(CMDFTP_WAR_MASK, mask); return 1; }

  rv = 1;
  for (i = 0; i < ld.count; i++) {
    char* match = strstr(ld.data[i].basename, from);
    
    if (match) {
      char* new_name; size_t new_len;

      new_len = strlen(ld.data[i].dirname) + 1 +
	strlen(ld.data[i].basename) - len_from + len_to;
      new_name = my_malloc(new_len + 1);

      *match = 0;

      sprintf(new_name, "%s/%s%s%s", ld.data[i].dirname, ld.data[i].basename,
	      to, match + len_from);

      if (!mf[mode][MODE_MOVE](new_name, ld.data[i].fullname))
	rv = 0;

      free(new_name);
    }
    if (!rv) break;
  }
  free_list(&ld);
  return rv;
}

void do_setcwd(int mode) {
  /* cast between function pointer types is always ok, as soon as
     we remember to cast back to the right type before using one. */
  char* (*fun_getcwd)(void) = (char* (*)(void))mf[mode][MODE_GETCWD];

  if (cwd[mode]) free(cwd[mode]);

  if ((cwd[mode] = fun_getcwd()))
    cwd[mode] = my_strdup(cwd[mode]);
  else if ((cwd[mode] = fun_getcwd()))
    cwd[mode] = my_strdup(cwd[mode]);
  else
    cmdftp_err(CMDFTP_ERR_PWD, "");
}

/**************************************/
/* DISPATCHED COMMANDS                */
/**************************************/

int cmd_quit(char** argv) { 
  cmdftp_war(CMDFTP_WAR_BYE, "");
  cleanexit();
  return 0;			/* never reached */
}

int cmd_h(char** argv) { 
  char* arg;
  int i;
  arg = argv[0];

  if (*arg != 0) cmdftp_war(CMDFTP_WAR_IARG, arg);

  for (i = 0; i < (sizeof(msg_help) / sizeof(char*)); i += 2)
    printf("%-14s %s\n", msg_help[i], msg_help[i + 1]);
  return 1;
}
    
int cmd_l(char** argv) {
  char* arg;
  arg = argv[0];

  if (*arg != 0) cmdftp_war(CMDFTP_WAR_IARG, arg);

  mode = CMDFTP_LOCAL;
  return 1;
}

int cmd_r(char** argv) {
  char* arg;
  arg = argv[0];

  if (*arg != 0) cmdftp_war(CMDFTP_WAR_IARG, arg);

  mode = CMDFTP_REMOTE;
  return 1;
}

int cmd_pwd(char** argv) {
  char* arg;
  arg = argv[0];

  if (*arg != 0) cmdftp_war(CMDFTP_WAR_IARG, arg);

  printf("%s\n", cwd[mode]);
  return 1;
}

int cmd_cd(char** argv) {
  char* arg;
  int rv;
  arg = argv[0];

  if (*arg == 0) { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }

  if ((rv = mf[mode][MODE_CHDIR](arg)) == -2)
    return 0;

  do_setcwd(mode);
  return (rv == 0);
}

int cmd_ren(char** argv) {
  int rv;

  if (*argv[0] == 0 || *argv[1] == 0)
    { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }

  rv = do_ren(argv[0], argv[1], argv[2]);
  return rv;
}

int cmd_rm(char** argv) {
  char* arg;
  struct list_data d;
  int i, rv = 0;
  arg = argv[0];

  if (*arg == 0) { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }
  
  init_list(&d);
  if (!mf[mode][MODE_FETCH_LIST](arg, &d)) return 0;
  if (d.count == 0) { cmdftp_war(CMDFTP_WAR_MASK, arg); return 1; }

  for (i = 0; i < d.count; i++) {
    int ft;
    if ((ft = mf[mode][MODE_FILE](d.data[i].fullname)) < 0 ||
	(ft == FILE_ISREG && !mf[mode][MODE_UNLINK](d.data[i].fullname)))
      break;
    if (i == d.count - 1) rv = 1;
  }
  free_list(&d);
  return rv;
}

int cmd_ls(char** argv) {
  char* arg;
  struct list_data d;
  int rv;
  arg = argv[0];

  init_list(&d);
  if (!mf[mode][MODE_FETCH_LIST](arg, &d)) return 0;
  if (d.count == 0) { cmdftp_war(CMDFTP_WAR_MASK, arg); return 1; }

  rv = ls(&d); free_list(&d); return rv;
}

int cmd_dir(char** argv) {
  char* arg;
  struct list_data d;
  int rv;
  arg = argv[0];

  if (*arg != 0) cmdftp_war(CMDFTP_WAR_IARG, arg);

  init_list(&d);
  if (!mf[mode][MODE_FETCH_PRETTY_LIST](&d)) return 0;
  if (d.count == 0) { cmdftp_war(CMDFTP_WAR_MASK, ""); return 1; }

  rv = ls(&d); free_list(&d); return rv;
}

int cmd_md(char** argv) {
  char* arg;
  arg = argv[0];

  if (*arg == 0) { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }
  return mf[mode][MODE_MKDIR](arg);
}

int cmd_rd(char** argv) {
  char* arg;
  arg = argv[0];

  if (*arg == 0) { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }
  return mf[mode][MODE_RMDIR](arg);
}


int cmd_p(char** argv) {
  char* arg;
  arg = argv[0];

  if (*arg == 0) { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }
  return mf[mode][MODE_PRINT](arg);
}

int cmd_e(char** argv) {
  char* arg;
  arg = argv[0];

  if (*arg == 0) { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }

  if (!env[CMDFTP_ENV_EDITOR])
    { cmdftp_war(CMDFTP_WAR_MENV, "EDITOR"); return 0; }

  return mf[mode][MODE_EDIT](arg);
}



/**************************************/
/* UPLOAD AND DOWNLOAD                */
/**************************************/


int cmd_u(char** paths) {
  char* source_mask;
  int rv; int tt; struct list_data ld;

  if (*paths[0] == 0 || *paths[1] == 0)
    { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }
  
  if ((tt = remote_file(paths[1])) < 0)
    return 0;
  
  if (tt != FILE_NEXIST && tt != FILE_ISDIR)
    { cmdftp_war(CMDFTP_WAR_TRG, "exists (not a dir)"); return 0; }

  source_mask = my_strdup(paths[0]);

  init_list(&ld);
  if (!local_fetch_list(paths[0], &ld)) return 0;

  if (ld.count == 0)
    { cmdftp_war(CMDFTP_WAR_MASK, paths[0]); return 1; }
  
  if (ld.count == 1) {
    int st = local_file(ld.data[0].fullname);
    if (st == FILE_ISDIR && tt == FILE_NEXIST) { 
      free(source_mask);
      source_mask = fullpath(paths[0], "*");
    }
  }

  free_list(&ld);
  rv = u_aux(paths[1], source_mask);
  free(source_mask);
  return rv;
}

int u_aux(char* target_dir, char* source_mask) {
  struct list_data ld;
  char* target[3];
  int i, rv;

  if ((rv = remote_file(target_dir)) == FILE_NEXIST) {
    if (!remote_mkdir(target_dir)) return 0;

  } else if (rv != FILE_ISDIR) {
    return 0;
  }

  init_list(&ld);

  if (!local_fetch_list(source_mask, &ld)) return 0;
  if (ld.count == 0) return 1;

  canonized_fn(target, target_dir);

  rv = 1;
  for (i = 0; i < ld.count; i++) {
    int file_type = local_file(ld.data[i].fullname);

    if (file_type == FILE_ISREG) {
      FILE* f = fopen(ld.data[i].fullname, "rb");
      if (!f)
	cmdftp_war(CMDFTP_WAR_OPEN, ld.data[i].fullname);
      else {
	char* fn;
	fn = fullpath(target[2], ld.data[i].basename);
	rv = upload(fn, f);
	fclose(f); free(fn);
      }
    } else if (file_type == FILE_ISDIR) {
      char* tmp_src, *tmp_trg;
      
      tmp_src = fullpath(ld.data[i].fullname, "*");
      tmp_trg = fullpath(target[2], ld.data[i].basename);
      
      rv = u_aux(tmp_trg, tmp_src);
      free(tmp_trg); free(tmp_src);

    } else {
      cmdftp_war(CMDFTP_WAR_SKIP, ld.data[i].fullname);
    }
    if (!rv) break;
  }

  free_list(&ld); free_fn(target);
  return rv;
}

int upload(char* target, FILE* source) {
  off_t start_pos, total_size;
  char* ctarget; char* op;
  size_t ctarget_len;
  int port, rv;

  rv = 0; 

  op = "Uploading"; 
  ctarget = clean_fn(target); ctarget_len = strlen(ctarget);

 start_transfer:
  
  if (!send_command("TYPE I", 0)) return 0;
  if ((total_size = local_size(source)) < 0) return 0;

  start_pos = 0;

  if (!(port = getport())) return 0;
  if (!(cmdftp_data = cmdftp_connect(port))) return 0;

  snprintf(cmd_buffer, CMD_BUF_SIZE, "STOR %s", target);
  if (send_command(cmd_buffer, 0) != 150) { close(cmdftp_data); return 0; }
  
  if (!o.q)
    print_progress(op, ctarget, ctarget_len, start_pos, total_size);

  transfer_interrupted = TRAN_INTR_NO;

  while (start_pos < total_size) {
    size_t toread, bytes; ssize_t written;

    toread = ((total_size - start_pos) < o.b) ? total_size - start_pos : o.b;

    if (!(bytes = fread(buffer, 1, toread, source))) break;
    if ((written = my_raw_write(buffer, bytes, cmdftp_data)) != bytes) break;

    start_pos += bytes;

    if (!o.q)
      print_progress(op, ctarget, ctarget_len, start_pos, total_size);
  }

  if (!o.q) fputc('\n', stdout);

  close(cmdftp_data); 

  if (start_pos < total_size) {
    if (ferror(source)) cmdftp_war(CMDFTP_WAR_READ, basename(target));
    else if TRANSFER_INTERRUPTED_CHECK("data transfer", 1);
    else { cmdftp_err(CMDFTP_ERR_UNER, strerror(errno)); }

  } else {
    if (!recv_confirm()) cmdftp_war(CMDFTP_WAR_NOCT, "");
    else rv = 1;
  }

  return rv;
}

int cmd_d(char** argv) {
  return do_d(argv, 0);
}

int cmd_dr(char** argv) {
  return do_d(argv, 1);
}

int do_d(char** paths, int resume) {
  char* source_mask;
  int rv; int tt; struct list_data ld;

  if (*paths[0] == 0 || *paths[1] == 0)
    { cmdftp_war(CMDFTP_WAR_MARG, ""); return 0; }

  if ((tt = local_file(paths[1])) != FILE_NEXIST && tt != FILE_ISDIR)
    { cmdftp_war(CMDFTP_WAR_TRG, "exists (not a dir)"); return 0; }

  source_mask = my_strdup(paths[0]);

  init_list(&ld);
  if (!remote_fetch_list(paths[0], &ld)) return 0;

  if (ld.count == 0)
    { cmdftp_war(CMDFTP_WAR_MASK, paths[0]); return 1; }
  
  if (ld.count == 1) {
    int st = remote_file(ld.data[0].fullname);
    if (st < 0) { free_list(&ld); return 0; }
    if (st == FILE_ISDIR && tt == FILE_NEXIST) { 
      free(source_mask);
      source_mask = fullpath(paths[0], "*");
    }
  }

  free_list(&ld);  
  rv = d_aux(paths[1], source_mask, resume);
  free(source_mask);
  return rv;
}

int d_aux(char* target_dir, char* source_mask, int resume) {
  struct list_data ld;
  char* target[3];
  int i, rv;

  if ((rv = local_file(target_dir)) == FILE_NEXIST) {
    if (!local_mkdir(target_dir)) return 0;

  } else if (rv != FILE_ISDIR) {
    return 0;
  }

  init_list(&ld);

  if (!remote_fetch_list(source_mask, &ld)) return 0;
  if (ld.count == 0) return 1;

  canonized_fn(target, target_dir);

  rv = 1;  
  for (i = 0; i < ld.count; i++) {
    int file_type;
    if ((file_type = remote_file(ld.data[i].fullname)) < 0) {
      rv = 0;

    } else if (file_type == FILE_ISREG) {
      char* fn; FILE* f;
      
      fn = fullpath(target[2], ld.data[i].basename);
      if (!(f = fopen(fn, resume ? "ab" : "wb")))
	cmdftp_war(CMDFTP_WAR_OPEN, fn);
      else {
	rv = download(f, ld.data[i].fullname);
	fclose(f);
      }
      free(fn);

    } else if (file_type == FILE_ISDIR) {
      char* tmp_src, *tmp_trg; 

      tmp_src = fullpath(ld.data[i].fullname, "*");
      tmp_trg = fullpath(target[2], ld.data[i].basename);
      rv = d_aux(tmp_trg, tmp_src, resume);
      free(tmp_trg); free(tmp_src);

    } else {
      cmdftp_war(CMDFTP_WAR_SKIP, ld.data[i].fullname);
    }

    if (!rv) break;
  }

  free_list(&ld); free_fn(target);
  return rv;
}

int download(FILE* target, char* source) {
  off_t start_pos, total_size;
  char* csource; char* op;
  size_t csource_len;
  int answer, port, rv;
  
  rv = 0;

  op = "Downloading"; 
  csource = clean_fn(source); csource_len = strlen(csource);
  
 start_transfer:

  if (!send_command("TYPE I", 0)) return 0;
  if ((total_size = remote_size(source)) < 0) return 0;
  if ((start_pos = ftello(target)) == -1) return 0;

  if (start_pos > 0) {
    snprintf(cmd_buffer, CMD_BUF_SIZE, "REST " OFF_FMT, start_pos);
    if ((answer = send_command(cmd_buffer, 0)) != 350) {
      cmdftp_war(CMDFTP_WAR_REST, "");
      fseeko(target, 0, SEEK_SET); start_pos = 0;
    }
  }

  if (!(port = getport())) return 0;
  if (!(cmdftp_data = cmdftp_connect(port))) return 0;

  snprintf(cmd_buffer, CMD_BUF_SIZE, "RETR %s", source);
  if (send_command(cmd_buffer, 0) != 150) { close(cmdftp_data); return 0; }

  if (!o.q)
    print_progress(op, csource, csource_len, start_pos, total_size);

  transfer_interrupted = TRAN_INTR_NO;

  while (start_pos < total_size) {
    ssize_t bytes; size_t toread, written;
   
    toread = ((total_size - start_pos) < o.b) ? total_size - start_pos : o.b;
    
    if ((bytes = my_raw_read(buffer, toread, cmdftp_data)) <= 0) break;
    if ((written = fwrite(buffer, 1, bytes, target)) != bytes) break;
    
    start_pos += bytes;
    
    if (!o.q)
      print_progress(op, csource, csource_len, start_pos, total_size);
  }

  if (!o.q) fputc('\n', stdout);

  close(cmdftp_data); 

  if (start_pos < total_size) {
    if (ferror(target))
      cmdftp_war(CMDFTP_WAR_WRIT, basename(source));
    else if TRANSFER_INTERRUPTED_CHECK("data transfer", 1);
    else { cmdftp_err(CMDFTP_ERR_UNER, strerror(errno)); }

  } else {
    if (!recv_confirm()) cmdftp_war(CMDFTP_WAR_NOCT, "");
    else rv = 1;
  }

  return rv;
}

/***********/
/* listing */
/***********/

int ls(struct list_data* d) {
  FILE* target; int pipe_fd[2];
  int i, rv = 0;

  if (env[CMDFTP_ENV_PAGER]) {
    if (pipe(pipe_fd) < 0) return 0;
    if (!(target = fdopen(pipe_fd[1], "w")))
      { close(pipe_fd[0]); close(pipe_fd[1]); return 0; }
  } else target = stdout;
  
  for (i = 0; i < d->count; i++)
    fprintf(target, "%s\n", d->data[i].fullname);
  
  if (!env[CMDFTP_ENV_PAGER]) return 1;
  
  fclose(target);
  rv = cmdftp_execute(env[CMDFTP_ENV_PAGER], 0, pipe_fd[0], -1);

  close(pipe_fd[0]);
  return rv;
}

/*****************************/
/* NETWORK RELATED FUNCTIONS */
/*****************************/


int cmdftp_connect(int port) {
  struct sockaddr_in address; int s, opvalue; socklen_t slen;
  
  opvalue = 8; slen = sizeof(opvalue);
  memset(&address, 0, sizeof(address));

  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ||
      setsockopt(s, IPPROTO_IP, IP_TOS, &opvalue, slen) < 0)
    return 0;

  address.sin_family = AF_INET;
  address.sin_port = htons((unsigned short)port);

  if (!server)
    if (!(server = gethostbyname(o.hostname))) return 0;

  memcpy(&address.sin_addr.s_addr, server->h_addr, server->h_length);
  
  if (connect(s, (struct sockaddr*) &address, sizeof(address)) == -1)
    return 0;

  return s;
}

void cmdftp_reconnect(void) {
  if (!(cmdftp_control = cmdftp_connect(o.p)))
    cmdftp_err(CMDFTP_ERR_CONN, "");
  greeting();
  if (!logging_in) {
    if (!login(user, pass)) cmdftp_err(CMDFTP_ERR_LGIN, "");
    if (cwd[CMDFTP_LOCAL])
      do_home(CMDFTP_LOCAL);
    if (cwd[CMDFTP_REMOTE])
      do_home(CMDFTP_REMOTE);
  }
}

ssize_t my_raw_read(char* buf, size_t n, int sc) { 
  ssize_t rv;

  rv = recv(sc, buf, n, 0);

  if (rv < 0) {
    if (transfer_interrupted != TRAN_INTR_INT)
      transfer_interrupted = TRAN_INTR_PIPE;
  }
  return rv;
}

ssize_t my_raw_write(char* buf, size_t n, int sc) {
  ssize_t rv;

  rv = send(sc, buf, n, 0);

  if (rv < 0) {
    if (transfer_interrupted != TRAN_INTR_INT)
      transfer_interrupted = TRAN_INTR_PIPE;
  }
  return rv;
}

int send_command(char* cmd, char suppress_err) { 
  char* s; size_t len, rv = 0;

  s = my_malloc((len = strlen(cmd) + 2) + 1);
  sprintf(s, "%s\r\n", cmd);

 start_transfer:
  transfer_interrupted = TRAN_INTR_NO;

  if (my_raw_write(s, len, cmdftp_control) < 0) {
    s[len - 2] = 0;
    if TRANSFER_INTERRUPTED_CHECK(s, 0);
  } else {
    s[len - 2] = 0;
    if (!(rv = recv_answer(0, 0, suppress_err))) {
      if TRANSFER_INTERRUPTED_CHECK(s, 1);
    }
  }
  free(s);
  return rv;
}

void reset_cmd_buffer(void) {
  cmd_ptr = cmd_buffer + CMD_BUF_SIZE;
}

char* recv_line(int sc) {
  char* new_ptr;
  ptrdiff_t n; size_t len;

  if (*cmd_ptr == 0) {
    ssize_t bytes = my_raw_read(cmd_buffer, CMD_BUF_SIZE, sc);
    if (bytes <= 0) return 0;
    cmd_buffer[bytes] = 0;
    cmd_ptr = cmd_buffer;
    if (*cmd_ptr == '\n' && cmd_buffer[CMD_BUF_SIZE - 1] == '\r') cmd_ptr++;
  }

  new_ptr = strchr(cmd_ptr, '\r');
  if (!new_ptr) new_ptr = cmd_buffer + CMD_BUF_SIZE;

  n = new_ptr - cmd_ptr;
  strncpy(cmd_line, cmd_ptr, n); cmd_line[n] = 0;
  cmd_ptr = new_ptr;

  if (cmd_ptr == cmd_buffer + CMD_BUF_SIZE) {
    ssize_t bytes = my_raw_read(cmd_buffer, CMD_BUF_SIZE, sc);
    if (bytes <= 0) return 0;
    cmd_buffer[bytes] = 0;

    cmd_ptr = cmd_buffer;

    new_ptr = strchr(cmd_ptr, '\r');
    if (!new_ptr) new_ptr = cmd_buffer + CMD_BUF_SIZE;
        
    n = new_ptr - cmd_ptr; len = strlen(cmd_line);
    strncat(cmd_line, cmd_ptr, n); cmd_line[len + n] = 0;
    cmd_ptr = new_ptr;
  }

  if (*cmd_ptr == '\r') {
    cmd_ptr++;
    if (*cmd_ptr == '\n') cmd_ptr++;
  }

  return cmd_line;
}

int recv_confirm(void) {
  int rv = 0;

 start_transfer:

  if (recv_answer(0, 0, 0) == 226) rv = 1;
  else if TRANSFER_INTERRUPTED_CHECK("recv_confirm", 1);

  return rv;
}

int recv_answer(int store, struct line_data* d, char suppress_err) {
  char* answer; int code; char str_code[] = { 0, 0, 0, ' ', 0 };

  reset_cmd_buffer();

  answer = recv_line(cmdftp_control);
  
  if (!answer || strlen(answer) < 3) return 0;

  strncpy(str_code, answer, 3);

  if (answer[3] == '-') {
    while ((answer = recv_line(cmdftp_control))) {       
      if (strncmp(answer, str_code, 4) == 0) break;
      if (store) store_line(answer, d);
      else printf("%s\n", answer);
    }
  }
  sscanf(str_code, "%i", &code);

  if (code >= 400 && !suppress_err) {
    cmdftp_war(CMDFTP_WAR_RERR, answer);
  }

  return code;
}

int getport() {
  unsigned int b[6]; 
  int i, answer; size_t len; char* port_str;

  if ((answer = send_command("PASV", 0)) != 227) return 0;

  port_str = cmd_buffer + 4;
  len = strlen(port_str += 4);

  for (i = 0; i < len; i++) 
    if (!isdigit(port_str[i])) port_str[i] = ' ';
  
  if (sscanf(port_str, "%u %u %u %u %u %u", b, b + 1, b + 2, b + 3, b + 4, b + 5) != 6) return 0;

  return b[4] * 256 + b[5];
}



/*******************************/
/* MEMORY AND STRING FUNCTIONS */
/*******************************/


void* my_malloc(size_t s) {
  void* rv;
  if (!(rv = malloc(s))) cmdftp_err(CMDFTP_ERR_HEAP, "");
  return rv;
}

void* my_realloc(void* ptr, size_t s) {
  void* rv;
  if (!(rv = realloc(ptr, s))) cmdftp_err(CMDFTP_ERR_HEAP, "");
  return rv;
}

#if !HAVE_STRDUP

char* strdup(char* s) {
  char* rv = malloc(strlen(s) + 1);
  strcpy(rv, s);
  return rv;
}

#endif

char* my_strdup(char* s) {
  char* rv;
  if (!(rv = strdup(s))) cmdftp_err(CMDFTP_ERR_HEAP, "");
  return rv;
}


/***************************/
/* OTHER UTILITY FUNCTIONS */
/***************************/

void cleanexit(void) {
  exit(fclose(stdout) == 0 ? 0 : -1);
}

void split_cmd(char* cmd, char** argv) {
  size_t len; int i;
  len = strlen(cmd);

  for (i = 0; i < 4; i++) {
    argv[i] = my_malloc(len + 1);
    while (isspace(*cmd)) cmd++;
    read_token(&cmd, argv[i]);
  }
}

void free_cmd(char** argv) {
  int i;

  for (i = 0; i < 4; i++) {
    if (argv[i]) {
      free(argv[i]); argv[i] = 0;
    }
  }
}  

int str_binsearch(char* key) {
  int low, high, mid, chk;

  low = 0; high = N_COMMANDS - 1;

  while (low <= high) {
    mid = (low + high) / 2;
    if ((chk = strcmp(key, commands[mid])) > 0) low = mid + 1;
    else if (chk < 0) high = mid - 1;
    else return mid;
  }

  return -1;
}

char* readline(int enable_tab, int enable_echo) {
  char* ptr; int c;

  ptr = cmd_userinput; memset(cmd_userinput, 0, sizeof(cmd_userinput));
  cmdftp_raw_mode();
  
  while (1) {
    c = fgetc(stdin);
    if (c == '\n')
      { *ptr = 0; fputc('\n', stdout); break; }
    else if ((c == '\t') && (enable_tab) &&
	     (ptr - cmd_userinput) < (CMD_BUF_SIZE - 10))
      { readline_tab(); ptr = cmd_userinput + strlen(cmd_userinput); }
    else if (c == cmdftp_termios.c_cc[VERASE] || c == 8)
      readline_bs(&ptr, enable_echo);
    else if ((c >= 32) && (c <= 127)) {
      if ((ptr - cmd_userinput) < (CMD_BUF_SIZE - 10))
	{ *ptr++ = c; if (enable_echo) fputc(c, stdout); }
    }
    else if (c == EOF) break;
  }
  cmdftp_canon_mode();
  return (c != EOF) ? my_strdup(cmd_userinput) : 0;
}

void readline_bs(char** ptr, int enable_echo) {
  if (*ptr > cmd_userinput) {
    *(--(*ptr)) = 0;
    if (enable_echo) fprintf(stdout, "\b \b");
  }
}

void readline_tab(void) {
  char* argv[4];
  char* mask, *completion; struct list_data ld;
  int c, i, j, lc_mode; size_t len, len_buffer;

  split_cmd(cmd_userinput, argv);
  if (!*argv[1])
    { free_cmd(argv); return; }

  mask = 0;
  c = i = j = len = len_buffer = 0;

  init_list(&ld);

  if (strcmp(argv[0], "d") == 0 || strcmp(argv[0], "dr") == 0) {
    lc_mode = *argv[2] ? CMDFTP_LOCAL : CMDFTP_REMOTE;
  } else if (strcmp(argv[0], "u") == 0) {
    lc_mode = *argv[2] ? CMDFTP_REMOTE : CMDFTP_LOCAL;
  } else lc_mode = mode;
  
  if (*(mask = argv[2]) != '\0') {
    char* tmp; tmp = escape_string(argv[1]);
    free(argv[1]); argv[1] = tmp;

  } else {
    mask = argv[1];
  }

  strcat(mask, "*");
  len_buffer = strlen(mask) - 1;
  
  if (!mf[lc_mode][MODE_FETCH_LIST](mask, &ld) || ld.count == 0) goto endtab;
  escape_list(&ld);
  len = strlen(ld.data[0].escaped_fullname);
  
  for (i = 0; i < len; i++) {
    c = ld.data[0].escaped_fullname[i];
    for (j = 1; j < ld.count; j++) {
      if (ld.data[j].escaped_fullname[i] != c) goto endcommon;
    }
  }
  
 endcommon:

 endtab:
  if (ld.count) {
    completion = my_malloc(i + 2);
    strncpy(completion, ld.data[0].escaped_fullname, i);

    if (ld.count == 1 &&
	mf[lc_mode][MODE_FILE](ld.data[0].fullname) == FILE_ISDIR)
      completion[i++] = '/';

    completion[i] = 0;

  } else {
    completion = my_malloc(len_buffer + 1);
    strncpy(completion, mask, len_buffer);
    completion[len_buffer] = 0;
  }
  
  snprintf(cmd_userinput, CMD_BUF_SIZE, "%s%s%s%s%s", argv[0],
	   *argv[1] ? " " : "", *argv[2] ? argv[1] : "", *argv[2] ? " " : "",
	   completion);
  free(completion);
  
  if (ld.count > 1) {
    fputc('\n', stdout);
    for (j = 0; j < ld.count; j++) {
      printf("%s\n", ld.data[j].escaped_fullname);
    }
  } else {
    fputc('\r', stdout);
  }

  print_prompt(); printf("%s", cmd_userinput);

  free_list(&ld);
  free_cmd(argv);
}

void cmdftp_pwd_start(void) {
  cmdftp_termios.c_lflag &= ~ECHO; tcsetattr(0, TCSANOW, &cmdftp_termios);
}

void cmdftp_pwd_end(void) {
  cmdftp_termios.c_lflag |= ECHO; tcsetattr(0, TCSANOW, &cmdftp_termios);
}

void cmdftp_raw_mode(void) {
  cmdftp_termios.c_lflag &= ~ICANON;
  cmdftp_termios.c_lflag &= ~ECHO;
  cmdftp_termios.c_cc[VMIN] = 1;
  cmdftp_termios.c_cc[VTIME] = 0;
  tcsetattr(0, TCSANOW, &cmdftp_termios);
}

void cmdftp_canon_mode(void) {
  cmdftp_termios.c_lflag |= ICANON; cmdftp_termios.c_lflag |= ECHO;
  tcsetattr(0, TCSANOW, &cmdftp_termios);
}

int cmdftp_execute(char* p1, char* p2, int read_fd, int write_fd) {
  char** argv = 0; char* argptr;
  pid_t pid; int status, rv, count;
  rv = count = 0;

  argptr = p1 = my_strdup(p1);

  do {
    argv = my_realloc(argv, sizeof(char*) * (count + 1));
    argv[count++] = argptr;
    argptr = strchr(argptr + 1, ' ');

    if (argptr) *argptr++ = 0;

  } while (argptr);

  count += (p2 ? 2 : 1); argv = my_realloc(argv, sizeof(char*) * count);
  argv[count - 1] = 0; if (p2) argv[count - 2] = p2;

  if ((pid = fork()) < 0) goto end_proc;
  else if (pid > 0) {
    if (waitpid(pid, &status, 0) > 0) {
      if (WIFEXITED(status) && (WEXITSTATUS(status) == 0)) rv = 1;
    }
  } else {
    if (read_fd != -1) { close(0); dup(read_fd); close(read_fd); }
    if (write_fd != -1) { close(1); dup(write_fd); close(write_fd); }
    execvp(argv[0], argv);
    exit(-1);
  }

 end_proc:
  free(p1); free(argv);
  return rv;
}

/* temporary file handling */

char* cmdftp_temp_fn;
size_t cmdftp_temp_fn_len;

int is_good_tmpdir(char* candidate) {
  struct stat buf;

  if (stat(candidate, &buf) != 0)
    return 0;
  
  return (S_ISDIR(buf.st_mode) && (buf.st_mode & S_IRWXU));
}

void init_temp(void) {
  if (env[CMDFTP_ENV_TMPDIR] && is_good_tmpdir(env[CMDFTP_ENV_TMPDIR])) {
    cmdftp_temp_fn = fullpath(env[CMDFTP_ENV_TMPDIR], "cmdftpXXXXXX");

#ifdef P_tmpdir
  } else if (is_good_tmpdir(P_tmpdir)) {
    cmdftp_temp_fn = fullpath(P_tmpdir, "cmdftpXXXXXX");
#endif

  } else if (is_good_tmpdir("/tmp")) {
    cmdftp_temp_fn = fullpath("/tmp", "cmdftpXXXXXX");

  } else {
    cmdftp_err(CMDFTP_ERR_TMPD, "");
  }

  cmdftp_temp_fn_len = strlen(cmdftp_temp_fn);
}

#if !HAVE_MKSTEMP

int mkstemp(char* template) {
#define MKSTEMP_ATTEMPTS 10
  int rv, attempt; size_t len; char *ptr; off_t n;
  char ascii[] = 
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

  len = strlen(template);	/* must be >= 6 */
  ptr = template + len - 6;     /* point to XXXXXX part */
  n = *((off_t*)o.hostname);	/* a crazy thing */
  if (n < 0) n = -n;

  for (attempt = 0; attempt < MKSTEMP_ATTEMPTS; attempt++) {
    int i;
    for (i = 0; i < 6; i++) {
      ptr[i] = ascii[n % (sizeof(ascii) - 1)]; n /= sizeof(ascii) - 1;
    }
    rv = open(template, O_RDWR | O_CREAT | O_EXCL, 0600);
    if (rv >= 0 || errno != EEXIST) return rv;
  }
  errno = EEXIST;
  return -1;
#undef MKSTEMP_ATTEMPTS
}

#endif

FILE* cmdftp_temp(char** fn) {
  int i; FILE* rv;

  *fn = 0;

  for (i = cmdftp_temp_fn_len - 6; i < cmdftp_temp_fn_len; i++)
    cmdftp_temp_fn[i] = 'X';

  if ((i = mkstemp(cmdftp_temp_fn)) < 0) return 0;
  if (!(rv = fdopen(i, "r+"))) return 0;
  *fn = my_strdup(cmdftp_temp_fn);
  return rv;
}

void canonized_fn(char* des[3], char* arg) {
  char* tmpmask;

  tmpmask = my_strdup(arg);
  des[0] = my_strdup(dirname(tmpmask));
  free(tmpmask);

  tmpmask = my_strdup(arg);
  des[1] = my_strdup(basename(tmpmask));
  free(tmpmask);

  des[2] = fullpath(des[0], des[1]);
}

/*****************************************************************************/

void free_fn(char* des[3]) {
  int i;
  for (i = 0; i < 3; i++) free(des[i]);
}

/*****************************************************************************/

char* clean_fn(char* fn) {
  if (strncmp(fn, "./", 2) == 0) return fn + 2;
  else if(strncmp(fn, "//", 2) == 0) return fn + 1;
  else return fn;
}

/*****************************************************************************/

char* fullpath(char* s, char* q) {
  char* rv = my_malloc(strlen(s) + strlen(q) + 2);
  sprintf(rv, "%s/%s", s, q);
  return rv;
}

/********************/
/* SIGNAL FUNCTIONS */
/********************/

void init_signals(void) {
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN; sigaction(SIGPIPE, &sa, 0); 
  sa.sa_handler = &handler_INT; sigaction(SIGINT, &sa, 0);
}

void handler_INT(int i) { 
  if (transfer_interrupted == TRAN_INTR_INT) {
    cmdftp_err(CMDFTP_ERR_INTR, "");

  } else {
    transfer_interrupted = TRAN_INTR_INT;
  }
}


/******************************/
/* STRUCT LINE_DATA FUNCTIONS */
/******************************/


void init_lines(struct line_data* d) {
  d->count = 0; d->lines = 0;
}

void init_list(struct list_data* d) {
  d->count = 0; d->data = 0;
}

void escape_list(struct list_data* d) {
  int i;
  for (i = 0; i < d->count; i++) {
    d->data[i].escaped_fullname =
      escape_string(d->data[i].fullname);
  }
}

char* escape_string(char* filestring) {
  char* src, *dst, *fs_new;
  size_t len;

  len = strlen(src = filestring);
  dst = fs_new = my_malloc(len * 2 + 1);

  do {
    if (*src == '\\') {
      *dst++ = '\\';
    } else if (*src == ' ') {
      *dst++ = '\\';
    }
  } while ((*dst++ = *src++));

  return fs_new;
}

void store_line(char* line, struct line_data* d) {
  int i = d->count;
  d->lines = my_realloc(d->lines, sizeof(char*) * (++d->count));
  d->lines[i] = my_malloc(CMD_BUF_SIZE - 9);
  strncpy(d->lines[i], line, CMD_BUF_SIZE - 10);
  d->lines[i][CMD_BUF_SIZE - 10] = 0; 
}

void store_list(char* fullname, struct list_data* d) {
  int i; char* base;
  base = basename(fullname);

  if ((o.d && (strcmp(base, ".") == 0 || strcmp(base, "..") == 0)) ||
      (!o.d && *base == '.')) return;
  
  i = d->count;
  d->data = my_realloc(d->data, sizeof(struct list_entry) * (++d->count));
  d->data[i].escaped_fullname = 0;
  d->data[i].fullname = my_strdup(fullname);
  d->data[i].basename = my_strdup(base);
  d->data[i].dirname = my_strdup(dirname(fullname));
}

void store_pretty_list(char* fullname, struct list_data* d) {
  int i = d->count;
  d->data = my_realloc(d->data, sizeof(struct list_entry) * (++d->count));
  d->data[i].escaped_fullname = 0;
  d->data[i].fullname = my_strdup(fullname);
  d->data[i].basename = d->data[i].dirname = 0;
}

void free_lines(struct line_data* d) {
  int i;
  if (d->lines) {
    for (i = 0; i < d->count; i++) free(d->lines[i]);
    free(d->lines); d->lines = 0;
  } 
  d->count = 0;
}

void free_list(struct list_data* d) {
  int i;
  if (d->data) {
    for (i = 0; i < d->count; i++) {
      if (d->data[i].escaped_fullname) free(d->data[i].escaped_fullname);
      if (d->data[i].fullname) free(d->data[i].fullname);
      if (d->data[i].basename) free(d->data[i].basename);
      if (d->data[i].dirname) free(d->data[i].dirname);
    }
    free(d->data); d->data = 0;
  }
  d->count = 0;
}

