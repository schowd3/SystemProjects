/**
** Siam Al-mer Chowdhury
**/
#define __USE_XOPEN_EXTENDED


#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include "shell.h"
#include "parse.h"

/* Constants */
#define DEBUG 1
/*
 * static const char *shell_path[] = { "./", "/usr/bin/", NULL };
 * static const char *built_ins[] = { "quit", "help", "kill",
 * "fg", "bg", "jobs", NULL};
*/

enum cmd_state {RUNNING, STOPPED};

struct job {
  int id;
  pid_t pid;
  enum cmd_state state; /* execution status - Running or Stopped */
  char * cmd_line;

  struct job * next;
};

/* Count of background jobs */
static int jobs_count = 0;
/* List of background jobs, in ascending id order */
static struct job * jobs = NULL, * jobs_tail = NULL;
/* Job running on the foreground */
static struct job * fgjob = NULL;

static struct job* jobs_find(const pid_t cpid){
  struct job * j = jobs;
  while(j){
    if(j->pid == cpid){
      return j;
    }
    j = j->next;
  }
  return NULL;
}

static struct job* jobs_find_id(const int id){
  struct job * j = jobs;
  while(j){
    if(j->id == id){
      return j;
    }
    j = j->next;
  }
  return NULL;
}

static void jobs_delete(const pid_t cpid){

  struct job * j = jobs;
  struct job * p = NULL;

  while(j){
    if(j->pid == cpid){
      break;
    }
    p = j;
    j = j->next;
  }

  if(p){
    p->next = j->next;
    if(j == jobs_tail){ /* if we remove the tail */
      jobs_tail = p;
      --jobs_count;
    }
  }else{ /* we remove the head*/
    if(jobs == jobs_tail){ /* if one node list */
      jobs_tail = j->next;
      --jobs_count;
    }
    jobs = j->next;
  }

  free(j->cmd_line);
  free(j);
}

static int jobs_add(const pid_t cpid, char * cmd){

  /* allocate the job struct */
  struct job * j = (struct job*) malloc(sizeof(struct job));
  if(j == NULL){
    perror("malloc");
    return -1;
  }

  /* setup the details */
  j->id = ++jobs_count;
  j->pid = cpid;

  const int len = strlen(cmd);
  j->cmd_line = (char*) malloc(sizeof(char)*(len+1));
  if(j->cmd_line == NULL){
    perror("malloc");
    free(j);
    return -1;
  }
  strncpy(j->cmd_line, cmd, len+1);

  j->state = RUNNING;

  /* add to background job list */
  if(jobs_tail){
    jobs_tail->next = j;
    jobs_tail = j;
  }else{
    jobs = jobs_tail = j;
  }
  j->next = NULL;

  return 0;
}

static int fgjob_add(const pid_t cpid, char * cmd){

  /* allocate the job struct */
  struct job * j = (struct job*) malloc(sizeof(struct job));
  if(j == NULL){
    perror("malloc");
    return -1;
  }

  /* setup the details */
  j->id = 0;  /* fg job has no id*/
  j->pid = cpid;

  const int len = strlen(cmd);
  j->cmd_line = (char*) malloc(sizeof(char)*(len+1));
  if(j->cmd_line == NULL){
    perror("malloc");
    free(j);
    return -1;
  }
  strncpy(j->cmd_line, cmd, len+1);

  j->state = RUNNING;
  j->next = NULL;

  if(fgjob){
    free(fgjob->cmd_line);
    free(fgjob);
  }
  fgjob = j;

  return 0;
}

static int cmd_waitpid(const pid_t cpid){

  int wstatus;
  do {
    const pid_t w = waitpid(cpid, &wstatus, 0);
    if (w == -1) {
      //perror("waitpid");
      break;
    }

    /*
    if (WIFEXITED(wstatus)) {
      rv = EXITSTATUS(wstatus);  //get exit code
    } else if (WIFSIGNALED(wstatus)) {
      rv = WTERMSIG(wstatus); //get termination signal
    }*/
  } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));
  return wstatus;
}

static int waitpid_report(const pid_t cpid, char * cmd, const int wstatus){
  int rv = 0;
  if (WIFEXITED(wstatus)) {
    log_job_fg_term(cpid, cmd);
    rv = WEXITSTATUS(wstatus);  //get exit code

  } else if (WIFSIGNALED(wstatus)) {

    if(WTERMSIG(wstatus) == SIGINT){
      log_ctrl_c();
    }

    log_job_fg_term_sig(cpid, cmd);
    rv = WTERMSIG(wstatus); //get termination signal

  } else if (WIFSTOPPED(wstatus)) {
    struct job * j;
    log_ctrl_z();

    tcsetpgrp(0, getpid());

    /* make the groground job background */
    jobs_add(fgjob->pid, fgjob->cmd_line);
    j = jobs_find(fgjob->pid);
    j->state = STOPPED;

    /* continue the now background job ? */
    //if(kill(fgjob->pid, SIGCONT) == -1){
    //  perror("kill");
    //  return 0;
    //}

    free(fgjob->cmd_line);
    free(fgjob);
    fgjob = NULL;

    log_job_fg_stopped(cpid, cmd);

#ifdef WCONTINUED
  } else if (WIFCONTINUED(wstatus)){  log_job_fg_cont(cpid, cmd);
#endif
  }
  return rv;
}

static int fg_waitpid(const pid_t cpid, char * cmd){

  int wstatus, rv, eint = 0;
  do {

    eint = 0;

    int flags = WUNTRACED;
#ifdef WCONTINUED
    flags |= WCONTINUED;
#endif

    const pid_t w = waitpid(cpid, &wstatus, flags);
    if (w == -1) {
      if((errno == EINTR) && (fgjob != NULL)){
        eint = 1;
        continue;
      }else{
        if(errno != EINTR)
          perror("waitpid");
        break;
      }
    }
    rv = waitpid_report(cpid, cmd, wstatus);

    if(WIFSTOPPED(wstatus)){
      break;
    }

  } while ((eint == 1) || (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus)));

  return rv;
}

static void exec_cmd_redirect(char *cmd_line, char *argv[], Cmd_aux *aux){

  /* if we have input redirection*/
  if(aux->in_file){
    stdin = freopen(aux->in_file, "r", stdin);
    if(stdin == NULL){
      log_file_open_error(aux->in_file);
      exit(1);
    }
  }

  if(aux->out_file){
    if(aux->is_append){
      stdout = freopen(aux->out_file, "a", stdout);
    }else{
      stdout = freopen(aux->out_file, "w", stdout);
    }

    if(stdout == NULL){
      log_file_open_error(aux->out_file);
      exit(1);
    }
  }
}

static int exec_cmd_external(char *cmd_line, char *argv[], Cmd_aux *aux){
  int i;
  char * cmd_path = NULL;
  const char * paths[2] = {".", "/usr/bin"};

  for(i = 0; i < 2; i++){

    /* check how much space we need */
    const int len = snprintf(NULL, 0, "%s/%s", paths[i], argv[0]);

    /* reallocate the buffer */
    cmd_path = realloc(cmd_path, sizeof(char)*(len+1));
    if(cmd_path == NULL){
      perror("realloc");
    }
    /* print command path */
    snprintf(cmd_path, len+1, "%s/%s", paths[i], argv[0]);

    /* try to execute */
    execv(cmd_path, argv);
    if(errno != ENOENT){
      log_command_error(cmd_line);
      break;
    }
  }

  //we get here only if command is not found in any of the paths
  log_command_error(cmd_line);
  return -1;
}

static int exec_fg(char * argv[]){
  sigset_t mask;
  sigset_t orig_mask;
  const pid_t id = atoi(argv[1]);

  /* try to find the job*/
  struct job * j = jobs_find_id(id);
  if(j == NULL){
    log_jobid_error(id);
    return -1;
  }

  log_job_fg(j->pid, j->cmd_line);

  /* if the job was stopped, we have to resume it */
  if(kill(j->pid, SIGCONT) == -1){
    perror("kill");
    return -1;
  }
  /* add to foreground and remove from jobs */
  fgjob_add(j->pid, j->cmd_line);
  jobs_delete(j->pid);


  sigemptyset (&mask);
  sigaddset (&mask, SIGCHLD);

  if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
    perror ("sigprocmask");
    return -1;
  }

  fg_waitpid(fgjob->pid, fgjob->cmd_line);

  if (sigprocmask(SIG_UNBLOCK, &orig_mask, NULL) < 0) {
    perror ("sigprocmask");
    return -1;
  }
  return 0;
}

static int exec_bg(char * argv[]){

  const pid_t id = atoi(argv[1]);

  /* try to find the job*/
  struct job * j = jobs_find_id(id);
  if(j == NULL){
    log_jobid_error(id);
    return -1;
  }

  log_job_bg(j->pid, j->cmd_line);

  /* if the job was stopped, we have to resume it */
  if(kill(j->pid, SIGCONT) == -1){
    perror("kill");
    return -1;
  }
  return 0;
}

static void exec_jobs(){

  /* Labels for job state */
  char* job_states[2] = {"Running", "Stopped"};

  log_job_number(jobs_count);

  /* Iterate over list and print job details*/
  struct job * j = jobs;
  while(j){
    log_job_details(j->id, j->pid, job_states[j->state], j->cmd_line);
    j = j->next;
  }
}

static void exec_kill(char *argv[]){
  const int sig = atoi(argv[1]);
  const pid_t cpid = atoi(argv[2]);

  log_kill(sig, cpid);

  if(kill(cpid, sig) == -1){
    perror("kill");
  }
}

void on_sigchld(){

  int wstatus = 0;
  do {

    int flags = WNOHANG | WUNTRACED;
#ifdef WCONTINUED
    flags |= WCONTINUED;
#endif

    const pid_t cpid = waitpid(-1, &wstatus, flags);
    if (cpid <= 0) {
      //perror("waitpid");
      break;
    }

    struct job * j = jobs_find(cpid);
    if(j == NULL){
      fprintf(stderr, "Error: Child %d not in jobs\n", cpid);
      continue;
    }

    if (WIFEXITED(wstatus)) {
      log_job_bg_term(cpid, j->cmd_line);
      jobs_delete(cpid);

    } else if (WIFSIGNALED(wstatus)) {

      log_job_bg_term_sig(cpid, j->cmd_line);
      jobs_delete(cpid);

    } else if (WIFSTOPPED(wstatus)) {
      j->state = STOPPED;
      log_job_bg_stopped(cpid, j->cmd_line);

#ifdef WCONTINUED
    } else if (WIFCONTINUED(wstatus)){
      j->state = RUNNING;
      log_job_bg_cont(cpid, j->cmd_line);
#endif
    }
  } while (wstatus > 0);

  tcsetpgrp(0, getpid());
}

static void on_sigttou(){
  if(tcsetpgrp(0, getpid()) == -1){
    perror("tcsetpgrp");
  }

  if(fgjob){
    kill(fgjob->pid, SIGTTOU);
  }
}

static void sig_handler(int sig){
  switch(sig){
    case SIGCHLD: on_sigchld(); break;
    //case SIGINT: on_sigint();   break;
    //case SIGTSTP: on_sigtstp(); break;
    case SIGTTOU: on_sigttou(); break;
  }
}

int exec_cmd(char *cmd_line, char *argv[], Cmd_aux *aux){
  int rv = 0;  /* success */

  if(strcmp(argv[0], "help") == 0){
    log_help();

  }else if(strcmp(argv[0], "quit") == 0){
    log_quit();
    rv = -1;

  }else if(strcmp(argv[0], "fg") == 0){
    exec_fg(argv);

  }else if(strcmp(argv[0], "bg") == 0){
    exec_bg(argv);

  }else if(strcmp(argv[0], "jobs") == 0){
    exec_jobs();

  }else if(strcmp(argv[0], "kill") == 0){
    exec_kill(argv);
  }else{

    sigset_t mask;
    sigset_t orig_mask;

    if(aux->is_bg == 0){
      sigemptyset (&mask);
      sigaddset (&mask, SIGCHLD);

      if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
        perror ("sigprocmask");
        return -1;
      }
    }

    const pid_t pid = fork();
    if(pid < 0){
      perror("fork");
      return -1;
    }else if(pid == 0){
      struct sigaction sa;

      if(aux->is_bg == 0){
        /* restore original mask in child */
        if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
          perror ("sigprocmask");
          return -1;
        }
      }

      /* child handles signal with default function */
      sa.sa_flags = 0;
      sigemptyset(&sa.sa_mask);
      sa.sa_handler = SIG_DFL;  /* use default signal handler */
      if( (sigaction(SIGCHLD, &sa, NULL) == -1) ||
          (sigaction(SIGINT, &sa, NULL) == -1) ||
          (sigaction(SIGTSTP, &sa, NULL) == -1)
          ){
        perror("sigaction");
        exit(-1);
      }

      exec_cmd_redirect(cmd_line, argv, aux);
      exec_cmd_external(cmd_line, argv, aux);
      exit(1);  /* child exit in case of error */

    }else{

      /* change process group of child */
      setpgid(pid, pid);

      if(aux->is_bg){
        jobs_add(pid, cmd_line);
        log_start_bg(pid, cmd_line);
      }else{

        tcsetpgrp(0, pid);

        /* set job as foreground */
        fgjob_add(pid, cmd_line);

        log_start_fg(pid, cmd_line);
        rv = fg_waitpid(pid, argv[0]);

        if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
      		perror("sigprocmask");
      		return -1;
      	}
      }
    }
  }

  return rv;
}

int exec_cmd_control(char *cmd_line, char *argv[], char *argv2[], Cmd_aux *aux){
  sigset_t mask;
  sigset_t orig_mask;
  int rv, wstatus;

  if(aux->is_bg == 0){
    sigemptyset (&mask);
    sigaddset (&mask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
      perror ("sigprocmask");
      return -1;
    }
  }

  const pid_t pid = fork();
  if(pid < 0){
    perror("fork");
    return -1;
  }else if(pid == 0){ /* cmd process */

    signal(SIGCHLD, SIG_DFL);

    /* fork the task process */
    const pid_t task_pid1 = fork();
    if(task_pid1 < 0){
      perror("fork");
      exit(1);
    }else if(task_pid1 == 0){ /* task child */

      exec_cmd_external(cmd_line, argv, aux);
      exit(1);  /* cmd-child exit in case of error */
    }else{
      if(aux->is_bg){
        log_start_bg(pid, cmd_line);
      }else{
        log_start_fg(task_pid1, cmd_line);
      }
      /* wait for the task to complete */
      wstatus = cmd_waitpid(task_pid1);
    }

    if (WIFEXITED(wstatus)) {
      rv = WEXITSTATUS(wstatus);  //get exit code
    } else if (WIFSIGNALED(wstatus)) {
      rv = WTERMSIG(wstatus); //get termination signal
    }

    /* decide what to do, based on control */
    if((aux->control == AND) && (rv != 0)){
      log_and_list(task_pid1, -1, cmd_line);
      rv = waitpid_report(task_pid1, cmd_line, wstatus);
      exit(rv);  /* cmd process exists with task exit code*/
    }else if((aux->control == OR) && (rv == 0)){
      log_or_list(task_pid1, -1, cmd_line);
      rv = waitpid_report(task_pid1, cmd_line, wstatus);
      exit(rv);  /* cmd process exists with task exit code*/
    }

    const pid_t task_pid2 = fork();
    if(task_pid2 < 0){
      perror("fork");
      exit(1);
    }else if(task_pid2 == 0){ /* task child */

      exec_cmd_external(cmd_line, argv2, aux);
      exit(1);  /* cmd-child exit in case of error */
    }else{

      if(aux->control == AND){
        log_and_list(task_pid1, task_pid2, cmd_line);
      }else if(aux->control == OR){
        log_or_list(task_pid1, task_pid2, cmd_line);
      }

      /* wait for the task to complete */
      rv = fg_waitpid(task_pid2, cmd_line);
    }
    exit(rv);  /* cmd process exists with task exit code*/

  }else{

    if(aux->is_bg){
      jobs_add(pid, cmd_line);
    }else{
      /* set job as foreground */
      fgjob_add(pid, cmd_line);
      cmd_waitpid(pid);

      if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
        perror ("sigprocmask");
        return -1;
      }
    }
  }

  return 0;
}

void setup_signals(){
  struct sigaction sa;

  sa.sa_flags = 0;
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = sig_handler;
  if( (sigaction(SIGCHLD, &sa, NULL) == -1) ||
      (sigaction(SIGINT, &sa, NULL) == -1) ||
      (sigaction(SIGTTOU, &sa, NULL) == -1) ||
      (sigaction(SIGTSTP, &sa, NULL) == -1) ){
    perror("sigaction");
    exit(-1);
  }
}

/* The entry of your shell program */
int main() {
  char cmdline[MAXLINE];        /* Command line */
  char *cmd = NULL;
  int quit = 0;

  /* Intial Prompt and Welcome */
  log_prompt();
  log_help();

  setup_signals();

  /* Shell looping here to accept user command and execute */
  while (!quit) {
    char *argv[MAXARGS], *argv2[MAXARGS];     /* Argument list */
    Cmd_aux aux;                /* Auxilliary cmd info: check parse.h */

    /* Print prompt */
    log_prompt();

    /* Read a line */
    // note: fgets will keep the ending '\n'
    if (fgets(cmdline, MAXLINE, stdin) == NULL) {
      if (errno == EINTR){
        if (feof(stdin)) {  /* ctrl-d will exit shell */
          exit(0);
        }
        printf("\n");
        continue;
      }
      exit(-1);
    }

    if (feof(stdin)) {  /* ctrl-d will exit shell */
      exit(0);
    }

    /* Parse command line */
    if (strlen(cmdline)==1)   /* empty cmd line will be ignored */
      continue;

    cmdline[strlen(cmdline) - 1] = '\0';        /* remove trailing '\n' */

    cmd = malloc(strlen(cmdline) + 1);
    snprintf(cmd, strlen(cmdline) + 1, "%s", cmdline);

    /* Bail if command is only whitespace */
    if(!is_whitespace(cmd)) {
      initialize_argv(argv);    /* initialize arg lists and aux */
      initialize_argv(argv2);
      initialize_aux(&aux);
      parse(cmd, argv, argv2, &aux); /* call provided parse() */

      if (DEBUG)  /* display parse result, redefine DEBUG to turn it off */
        debug_print_parse(cmd, argv, argv2, &aux, "main (after parse)");

      /* After parsing: your code to continue from here */
      /*================================================*/
      if(aux.control == NONE){
        if(exec_cmd(cmd, argv, &aux) == -1){
          quit = 1;
        }
      }else{
        if(exec_cmd_control(cmd, argv, argv2, &aux) == -1){
          quit = 1;
        }
      }
    }

    free_options(&cmd, argv, argv2, &aux);
  }
  return 0;


}
