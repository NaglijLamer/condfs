# Condfs
Pseudo file system for threads locked on condition variables.

# Structure
Display all locked threads as files.
This files are readable (output is `wmesg` of condition variable) and writable (input *signal* will send `cv_signal` to condition variable locking this thread, other input is ignored).
Internal cache of condinodes is used for storing vnodes associated with files.

# Known problems.
## 1. Release of vnodes.
### Explanation.
If thread with some thread id will never again use any condition variable and its process will not die - condinode will be still alive and keep vnode.
### Current soulution.
File system can't have more condinodes, than maximum amount of threads in system. The most of threads will use again some condition variable (and reuse vnode) or their process will die and trigger proc_exit event (and destroy condinode with release of vnode).

## 2. Reading from pseudofiles.
### Explanation.
It is a possible situation, when state of string `wmesg` changes during read operation. For example, VFS calls VOP_READ operation with offset 0. It stores 'select' to uiobuffer. At this moment the state of thread changes and thread is locked on another condition variable. VFS tries to calls VOP_READ operation of condfs with offset 6. It should return empty buffer, because there is nothing to read. But if new value of wmesg is longer than previous ('select') it will return the rest of new string `wmesg`.
### Current solution.
Probable, it is a correct behaviour of file system and can't be changed without awful code lines. 
