syslog
======

The lib implements a RFC5424 compliant syslog interface for ESP8266. The syslog
message is send via UDP.

syslog messages are queued on heap until the Wifi stack is fully initialized.
If the remaining heap size reaches a given limit, syslog will add a final obituary
and stop further logging until the queue is empty and sufficient heap space is
available again.

The module may be controlled by flashconfig variables:

* **syslog_host: host[:port]**

    **host** is an IP-address or DNS-name. **port** is optional and defaults to 514.
DNS-Resolution is done as soon as the Wifi stack is up and running.

* **syslog_minheap: 8192**

    **minheap** specifies the minimum amount of remaining free heap when queuing up
syslog messages. If the remaining heap size is below **minheap**, syslog will insert
an obituary message and stop queuing. After processing all queued messages, the
logging will be enabled again.

* **syslog_filter: 0..7**

    **syslog_filter** is the minimum severity for sending a syslog message. The filter
is applied against the message queue, so any message with a severity numerical lower
than **syslog_filter** will be dropped instead of being send.

* **syslog_showtick: 0|1**

    If **syslog_showtick** is set to **1**, syslog will insert an additional timestamp
(system tick) as "PROCESS" field (before the users syslog message).
The value shown is in ms, (1µs resolution) since (re)boot or timer overflow.

* **syslog_showdate: 0|1**

    If **syslog_showdate** is set to **1**, syslog will insert the ESPs NTP time
into the syslog message. If "realtime_stamp" (NTP 1s ticker) is **NULL**, the
time is derived from a pseudo-time based on the absolute value of systemticks.

    Some syslog servers (e.g. Synology) will do crazy things if you set **syslog_showdate** to **1**


The syslog module exports two functions:

```
syslog_init(char *server_name);
syslog(uint8_t facility, uint8_t severity, const char *tag, const char *fmt, ...);
```

syslog_init
-----------
usage: `syslog_init(char *server_name);`

**syslog_init** expects a server name in format "host:port" (see **syslog_host** flashconfig).

If **server_name** is **NULL**, all dynamic allocated memory (buffers, queues, interfaces)
are released and the syslog state is set to "SYSLOG_HALTED".

If **server_name** is **""**, syslog state is set to "SYSLOG_HALTED", without clearing
the queue.

Otherwise, syslog_init will allocate all required structures (buffers, interfaces) and
send all collected syslog messages.

syslog is self-initializing, meaning the syslog_init(server_name) is called on first
invocation. The syslog_init function is only for convenience if you have to stop or disable syslog functions.


syslog
------
usage: `syslog(uint8_t facility, uint8_t severity, const char *tag, const char *fmt, ...);`

* **facility**

    the message facility (see syslog.h, **enum syslog_facility**).

* **severity**

    the message severity (see syslog.h, **enum syslog_severity**)

* **tag**

    user defined tag (e.g. "MQTT", "REST", "UART") to specify where the message belongs to

* ** const char *fmt, ...**

    the desired message, in printf format.

Examples
========

    showtick=0, showdate=0
    Invocation: syslog()
output:

    showtick=1, showdate=1
    Invocation: syslog()
output:

    showtick=1, showdate=1, NTP not available
    Invocation: syslog()
output:

    showtick=1, showdate=1, NTP available
    Invocation: syslog()
output:
