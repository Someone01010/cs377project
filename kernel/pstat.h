#ifndef _PSTAT_H_
#define _PSTAT_H_

// r_time() counts at 10 MHz on QEMU's virt machine.
#define TIMEBASE_HZ 10000000UL

struct pstat {
  int pid;
  uint64 ctime;
  uint64 first_run;
  uint64 etime;
  uint64 run_time;
  uint64 wait_time;
  uint64 sleep_time;
  uint64 nsched;
};

#endif