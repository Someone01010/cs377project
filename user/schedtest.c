#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/pstat.h"
#include "user/user.h"

#define MAXKIDS 8

struct job {
  char *label;
  int io;
  int millions;
};

// Returns x so the compiler does not reject it as set but unused (-Werror).
static unsigned long
cpu_work(int millions)
{
  volatile unsigned long x = 0;
  for (long i = 0; i < (long)millions * 1000000L; i++)
    x += i;
  return x;
}

static void
io_work(int rounds, int millions_per_round)
{
  for (int r = 0; r < rounds; r++) {
    cpu_work(millions_per_round);
    pause(1);
  }
}

static int
us(uint64 t)
{
  return (int)(t / (TIMEBASE_HZ / 1000000UL));
}

static void
num(int v, int w)
{
  int digits = 1;
  for (int t = v; t >= 10; t /= 10)
    digits++;
  for (int i = digits; i < w; i++)
    printf(" ");
  printf("%d", v);
}

static void
header(void)
{
  printf("%s\n", "  pid  workload    turnaround  response       cpu      wait     sleep  #sched");
  printf("%s\n", "                        (us)      (us)      (us)      (us)      (us)");
}

static void
row(int pid, char *label, int turnaround, int response, int cpu, int wait,
    int sleepus, int nsched)
{
  num(pid, 5);
  printf("  %s", label);
  num(turnaround, 11);
  num(response, 10);
  num(cpu, 10);
  num(wait, 10);
  num(sleepus, 10);
  num(nsched, 8);
  printf("\n");
}

static void
run_experiment(char *title, int n, struct job *jobs)
{
  int pids[MAXKIDS];

  printf("\n=== %s ===\n", title);

  for (int i = 0; i < n; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("schedtest: fork failed\n");
      exit(1);
    }
    if (pid == 0) {
      if (jobs[i].io)
        io_work(8, jobs[i].millions / 8 > 0 ? jobs[i].millions / 8 : 1);
      else
        cpu_work(jobs[i].millions);
      exit(0);
    }
    pids[i] = pid;
  }

  header();
  int sum_ta = 0, sum_rt = 0, sum_wt = 0;
  int max_ta = 0, max_rt = 0, max_wt = 0, max_other = 0;
  for (int i = 0; i < n; i++) {
    struct pstat st;
    int status;
    int pid = wait_stat(&status, &st);
    if (pid < 0) {
      printf("schedtest: wait_stat failed\n");
      exit(1);
    }
    char *label = "?         ";
    for (int j = 0; j < n; j++)
      if (pids[j] == pid)
        label = jobs[j].label;

    int turnaround = us(st.etime - st.ctime);
    int response = us(st.first_run - st.ctime);
    int cpu = us(st.run_time);
    int wait = us(st.wait_time);
    int sleepus = us(st.sleep_time);
    int other = turnaround - cpu - wait - sleepus;
    if (other > max_other)
      max_other = other;
    row(pid, label, turnaround, response, cpu, wait, sleepus,
        (int)st.nsched);

    sum_ta += turnaround;
    sum_rt += response;
    sum_wt += wait;
    if (turnaround > max_ta)
      max_ta = turnaround;
    if (response > max_rt)
      max_rt = response;
    if (wait > max_wt)
      max_wt = wait;
  }

  printf("  average: turnaround %d  response %d  wait %d (us)\n", sum_ta / n,
         sum_rt / n, sum_wt / n);
  printf("  maximum: turnaround %d  response %d  wait %d (us)\n", max_ta,
         max_rt, max_wt);
  printf("  unaccounted (fork setup) per child, at most: %d us\n", max_other);
}

int
main(int argc, char *argv[])
{
  int millions = 150;
  if (argc > 1)
    millions = atoi(argv[1]);
  if (millions < 5)
    millions = 5;
  int small = millions / 5;

  struct job e1[4] = {
    {"cpu-bound ", 0, millions},
    {"cpu-bound ", 0, millions},
    {"cpu-bound ", 0, millions},
    {"cpu-bound ", 0, millions},
  };
  run_experiment("Experiment 1: 4 identical CPU-bound children", 4, e1);

  struct job e2[4] = {
    {"long      ", 0, millions},
    {"short     ", 0, small},
    {"short     ", 0, small},
    {"short     ", 0, small},
  };
  run_experiment("Experiment 2: 1 long job, then 3 short jobs", 4, e2);

  struct job e3[3] = {
    {"cpu-bound ", 0, millions},
    {"cpu-bound ", 0, millions},
    {"io-like   ", 1, small},
  };
  run_experiment("Experiment 3: 2 CPU-bound + 1 I/O-like child", 3, e3);

  printf("\nturnaround = exit - creation        response = first run - creation\n");
  printf("cpu = time RUNNING                  wait = time RUNNABLE (ready, not on CPU)\n");
  printf("sleep = time SLEEPING (tracked in the kernel)\n");
  printf("unaccounted = turnaround - cpu - wait - sleep: fork() setup in USED state\n");
  printf("schedtest: done (times in microseconds; timebase %d MHz)\n",
         (int)(TIMEBASE_HZ / 1000000UL));
  exit(0);
}