#include <linux/string.h>
#include <linux/timer.h>

#define strncpy(dest, src, size) ({ strscpy_pad((dest), (src), (size)); (dest); })
#define del_timer(timer) timer_delete(timer)
#define wakeup_source_remove(source) wakeup_source_unregister(source)
#define PDE_DATA pde_data
