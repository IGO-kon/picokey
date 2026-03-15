// Compatibility layer for toolchains that enable newlib retargetable locking
// but don't provide lock hook implementations.

struct __lock {
    long long x;
};

struct __lock __lock___arc4random_mutex;
struct __lock __lock___atexit_recursive_mutex;
struct __lock __lock___at_quick_exit_mutex;
struct __lock __lock___env_recursive_mutex;
struct __lock __lock___malloc_recursive_mutex;
struct __lock __lock___sfp_recursive_mutex;
struct __lock __lock___tz_mutex;

void __retarget_lock_init(void *lock)
{
    (void)lock;
}

void __retarget_lock_init_recursive(void *lock)
{
    (void)lock;
}

void __retarget_lock_close(void *lock)
{
    (void)lock;
}

void __retarget_lock_close_recursive(void *lock)
{
    (void)lock;
}

void __retarget_lock_acquire(void *lock)
{
    (void)lock;
}

void __retarget_lock_acquire_recursive(void *lock)
{
    (void)lock;
}

int __retarget_lock_try_acquire(void *lock)
{
    (void)lock;
    return 1;
}

int __retarget_lock_try_acquire_recursive(void *lock)
{
    (void)lock;
    return 1;
}

void __retarget_lock_release(void *lock)
{
    (void)lock;
}

void __retarget_lock_release_recursive(void *lock)
{
    (void)lock;
}
