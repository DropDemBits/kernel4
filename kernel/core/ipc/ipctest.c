#include <string.h>

#include <common/ipc/message.h>
#include <common/sched/sched.h>
#include <common/tasks/tasks.h>
#include <common/util/klog.h>
#include <common/mm/liballoc.h>

void recv_thread(void* params)
{
    thread_t* sibling_thread;
    struct ipc_message* recving_msg = NULL;

    sched_sleep_ms(1000);

    sibling_thread = params;

    klog_logln(LVL_INFO, "Other Thread: %s", sibling_thread->name);

    do
    {
        msg_recv(MSG_TYPE_DATA, &recving_msg, MSG_XACT_SYNC, 0);
        sched_sleep_ms(100);
    }
    while(!recving_msg);

    klog_log(LVL_INFO, "T1: Message recv! (%s, %p)", recving_msg->data, recving_msg->data);

    sched_terminate();
}

void ipc_test()
{
    thread_t* sibling_thread;
    struct ipc_message* sending_msg = NULL;
    char* test_buffer;
    int status;

    klog_logln(LVL_INFO, "Testing IPC:");

    taskswitch_disable();
    // Pass current thread to sibling
    sibling_thread = thread_create(sched_active_process(), (void*)recv_thread, PRIORITY_NORMAL, "recv_thread", sched_active_thread());
    taskswitch_enable();

    // Fill test buffer
    test_buffer = kmalloc(64);
    strcpy(test_buffer, "Hello, World!");

    // Build & send other message
    sending_msg = kmalloc(sizeof(*sending_msg));
    build_msg(sending_msg, MSG_TYPE_DATA, test_buffer, 64);
    status = msg_send(sibling_thread, sending_msg, MSG_XACT_SYNC, 0);

    klog_log(LVL_INFO, "T0: Message sent! (%d, %p)", status, test_buffer);
}
