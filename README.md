# libfutex

## References

The following are references about (non-robust) futexes:

- [futex(2)](https://man7.org/linux/man-pages/man2/futex.2.html)
- [futex(7)](https://man7.org/linux/man-pages/man7/futex.7.html)
- [Futexes Are Tricky](https://cis.temple.edu/~giorgio/cis307/readings/futex.pdf)
- [Futex Cheat Sheet](http://locklessinc.com/articles/futex_cheat_sheet/)

The following are references about **robust** futexes on the side of the Linux
kernel:

- [set_robust_list(2)](https://man7.org/linux/man-pages/man2/set_robust_list.2.html)
- [A description of what robust futexes are](https://docs.kernel.org/locking/robust-futexes.html)
- [The robust futex ABI](https://docs.kernel.org/locking/robust-futex-ABI.html)
- [Robust futexes - a new approach](https://lwn.net/Articles/172149/)

There aren't many references on the internet about the use of **robust**
futexes (specifically `robust_list`). The only implementation I found is from
glibc. The following is a summary of the information contained in
the [glibc-2.36](https://github.com/bminor/glibc/tree/glibc-2.36)
source code.

- `struct robust_list_head` is an userspace per-thread data structure used to
  keep track of futexes that the thread has acquired. It is registered with the
  Linux kernel
  via [`set_robust_list`](https://man7.org/linux/man-pages/man2/set_robust_list.2.html)
  so that when the thread exits (either normally or abnormally), the kernel can
  iterate through the list and release the futexes that the thread has
  acquired. `struct robust_list_head` is defined
  in [`linux/futex.h`](https://github.com/torvalds/linux/blob/8cb1ae19bfae92def42c985417cd6e894ddaa047/include/uapi/linux/futex.h#L89-L122)
  as part of the Linux syscall ABI.

- In glibc, the `struct robust_list_head` is stored in the `robust_list`
  field
  of [`struct pthread`](https://github.com/bminor/glibc/blob/4e21c2075193e406a92c0d1cb091a7c804fda4d9/nptl/descr.h#L179),
  which is the struct pointed to by `pthread_t`.

- When a process first starts, `robust_head` is initialized and registered
  with the kernel with `set_robust_list` system call. The relevant code is in
  [`__tls_init_tp`](https://github.com/bminor/glibc/blob/4e21c2075193e406a92c0d1cb091a7c804fda4d9/sysdeps/nptl/dl-tls_init_tp.c#L84-L101):

    ```c
    #0  __tls_init_tp () at ../sysdeps/nptl/dl-tls_init_tp.c:106
    #1  0x00007ffff7fe3e28 in init_tls (naudit=naudit@entry=0) at ./elf/rtld.c:821
    #2  0x00007ffff7fe766c in dl_main (phdr=<optimized out>, phnum=<optimized out>, user_entry=<optimized out>,
    auxv=<optimized out>) at ./elf/rtld.c:2045
    #3  0x00007ffff7fe285c in _dl_sysdep_start (start_argptr=start_argptr@entry=0x7fffffffdd90,
    dl_main=dl_main@entry=0x7ffff7fe4900 <dl_main>) at ../elf/dl-sysdep.c:256
    #4  0x00007ffff7fe45b8 in _dl_start_final (arg=0x7fffffffdd90) at ./elf/rtld.c:507
    #5  _dl_start (arg=0x7fffffffdd90) at ./elf/rtld.c:596
    #6  0x00007ffff7fe32b8 in _start () from /lib64/ld-linux-x86-64.so.2
    ```
- When a new thread is created, tht content of the `robust_list` is initialized
  in
  [`allocate_stack`](https://github.com/bminor/glibc/blob/4e21c2075193e406a92c0d1cb091a7c804fda4d9/nptl/allocatestack.c#L545-L555):

    ```c
    #0  allocate_stack (stacksize=<synthetic pointer>, stack=<synthetic pointer>, pdp=<synthetic pointer>, attr=0x7fffffffdb50)
    at ./nptl/allocatestack.c:555
    #1  __pthread_create_2_1 (newthread=<optimized out>, attr=<optimized out>, start_routine=<optimized out>, arg=<optimized out>)
    at ./nptl/pthread_create.c:647
    ```
  Then `set_robust_list` is called in
  [`start_thread`](https://github.com/bminor/glibc/blob/4e21c2075193e406a92c0d1cb091a7c804fda4d9/nptl/pthread_create.c#L382-L387),

    ```c
    #0  start_thread (arg=<optimized out>) at ./nptl/pthread_create.c:400
    #1  create_thread (pd=pd@entry=0x7ffff7d7e640, attr=attr@entry=0x7fffffffdb50, 
    stopped_start=stopped_start@entry=0x7fffffffdb4e, stackaddr=stackaddr@entry=0x7ffff757e000, stacksize=8388352, 
    thread_ran=thread_ran@entry=0x7fffffffdb4f) at ./nptl/pthread_create.c:285
    #2  0x00007ffff7e17280 in __pthread_create_2_1 (newthread=<optimized out>, attr=<optimized out>, start_routine=<optimized out>,
    arg=<optimized out>) at ./nptl/pthread_create.c:828
    ```
- When a mutex is acquired, it needs to be added to the `robust_list` of the
  current thread so the kernel can see it when the thread exits. Similarly, when
  a mutex is released, it should be removed. The relevant code is
  in [nptl/descr.h](https://github.com/bminor/glibc/blob/4e21c2075193e406a92c0d1cb091a7c804fda4d9/nptl/descr.h#L178-L227)

- See
  [pthread_mutex_lock.c](https://github.com/bminor/glibc/blob/4e21c2075193e406a92c0d1cb091a7c804fda4d9/nptl/pthread_mutex_lock.c)
  and [pthread_mutex_unlock.c](https://github.com/bminor/glibc/blob/4e21c2075193e406a92c0d1cb091a7c804fda4d9/nptl/pthread_mutex_unlock.c)
  for the implementation of `pthread_mutex_lock` and `pthread_mutex_unlock`.
