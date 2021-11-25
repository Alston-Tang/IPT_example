#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <iostream>
#include <vector>
#include <string>

struct perf_record_aux_record {
    struct perf_event_header header;
    uint64_t aux_offset;
    uint64_t aux_size;
    uint64_t flag;
};

void print_perf_event_mmap_info(struct perf_event_mmap_page* mmap_page) {
    std::cout << "version: " << mmap_page->version << std::endl;
    std::cout << "time enabled: " << mmap_page->time_enabled << std::endl;
    std::cout << "time running: " << mmap_page->time_running << std::endl;
    std::cout << "cap_user_time: " << mmap_page->cap_user_time << std::endl;
    std::cout << "cap_user_time_zero: " << mmap_page->cap_user_time_zero << std::endl;
    std::cout << "data_size: " << mmap_page->data_size << std::endl;
    std::cout << "data_offset: " << mmap_page->data_offset << std::endl;
    std::cout << "data_head: " << mmap_page->data_head << std::endl;
    std::cout << "data_tail: " << mmap_page->data_tail << std::endl;
    std::cout << "aux_size: " << mmap_page->aux_size << std::endl;
    std::cout << "aux_offset: " << mmap_page->aux_offset << std::endl;
    std::cout << "aux_head: " << mmap_page->aux_head << std::endl;
    std::cout << "aux_tail: " << mmap_page->aux_tail << std::endl;
}

bool stop_tracing = false;

void stop_tracing_handler(int sig) {
  if (sig == SIGINT) {
    stop_tracing = true;
  }
}

int main() {
    int ret = 0;
    int fd = -1;
    void* ring_buffer = NULL;
    void* aux_buffer = NULL;
    struct perf_event_mmap_page* mmap_page = NULL;
    struct perf_event_attr attr = {};

    char c;

    // set signal handler to stop loop
    auto handler = signal(SIGINT, stop_tracing_handler);
    if (handler == SIG_ERR) {
      ret = errno;
      goto error;
    }

    std::cout << "self pid: " << getpid() << std::endl;
    attr.type = 8;
    attr.size = sizeof(attr);
    // branch | pt
    attr.config = 0x2001;
    attr.sample_period = 1;
    attr.disabled = 1;
    attr.inherit = 1;

    std::cout << "perf_event_open..." << std::endl;
    fd = syscall(SYS_perf_event_open, &attr, -1, 0, -1, 0);
    if (fd < 0) {
        ret = errno;
        goto error;
    }

    std::cout << "mmap main ring buffer..." << std::endl;
    ring_buffer = mmap(NULL, 4096 + 4 * 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ring_buffer == MAP_FAILED) {
        ret = errno;
        goto error;
    }

    mmap_page = (struct perf_event_mmap_page*) ring_buffer;

    mmap_page->aux_offset = 4096 + 4096 * 4096;
    // 256 MiB
    mmap_page->aux_size = 4096 * 4096 * 8;
    // print_perf_event_mmap_info(mmap_page);

    std::cout << "mmap aux ring buffer (size=" << mmap_page->aux_size << ")..." << std::endl;
    aux_buffer = mmap(ring_buffer, mmap_page->aux_size, PROT_READ, MAP_SHARED, fd, mmap_page->aux_offset);
    if (aux_buffer == MAP_FAILED) {
        std::cerr << "failed to allocate aux ring buffer" << std::endl;
        ret = errno;
        goto error;
    }

    print_perf_event_mmap_info(mmap_page);

    std::cout << "enable intel pt..." << std::endl;
    if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) != 0) {
        ret = errno;
        goto error;
    }

    std::cout << "Ctrl+C to stop tracing" << std::endl;
    while (!stop_tracing) {
	      int data_head = mmap_page->aux_head;
	      asm volatile("lfence":::"memory");
	      mmap_page->aux_tail = mmap_page->aux_head;
	      usleep(1000);
    }

    /*
    std::cout << "record for 3 seconds..." << std::endl;
    sleep(3);
    */
    std::cout << "disable intel pt..." << std::endl;
    if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0) != 0) {
        ret = errno;
	      goto error;
    }
    print_perf_event_mmap_info(mmap_page);
    /*
    //debug
    fd = open("/home/alston64/output", O_WRONLY | O_CREAT);
    if (fd < 0) {
        ret = errno;
	goto error;
    }
    write(fd, aux_buffer, mmap_page->aux_head);
    return 0;
    */

    //debug
    while (mmap_page->data_head > mmap_page->data_tail) {
        struct perf_event_header* header = (struct perf_event_header*)((uint8_t*)ring_buffer + mmap_page->data_offset + mmap_page->data_tail);
	      std::cout << "type: " << header->type << " size: " << header->size << std::endl;
	      if (header->type == PERF_RECORD_AUX) {
	          struct perf_record_aux_record* aux_header = (struct perf_record_aux_record*)header;
	          std::cout << "aux_offset: " << aux_header->aux_offset << " aux_size: " << aux_header->aux_size << " truncate: " << (int)((aux_header->flag & PERF_AUX_FLAG_TRUNCATED) > 0) << " overrite: " << (int)((aux_header->flag & PERF_AUX_FLAG_OVERWRITE) > 0) << std::endl;
	      }
	      mmap_page->data_tail += header->size;
    }

    return 0;
error:
    std::cerr << strerror(ret) << std::endl;
    return ret;
}
