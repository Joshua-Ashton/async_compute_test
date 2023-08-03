gcc -lvulkan -lcap main.c -oasync_compute_test
sudo setcap CAP_SYS_NICE=eip async_compute_test
