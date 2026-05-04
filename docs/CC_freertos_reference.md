FreeRTOS is the small real-time operating system running underneath Arduino on the ESP32.

In plain terms: it lets the ESP32 run multiple “tasks” that take turns using the processor. The ESP32 has two cores, and FreeRTOS manages things like:

running the main Arduino loop()
running Wi-Fi/network internals
running background tasks we create
scheduling which task gets CPU time
queues, locks/mutexes, timers, delays
So instead of doing everything inside one giant loop(), we can create a separate task like:

dataWorkerTask
and let that task handle slow network calls while the normal loop keeps reading touch and updating the screen.

It is not a full desktop OS. There are no windows, filesystems, users, etc. It’s a lightweight scheduler for microcontrollers. Arduino on ESP32 already uses it; we’re just explicitly using one of its features.