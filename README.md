# Wall Broadcaster

Wall Broadcaster is a service which registers itself as session with
`systemd-logind`, listens to it's TTY for `wall` messages and forwards
them via D-BUS. Clients, e.g. for graphical desktops, can listen to it
and display the messages to the user.

In the past, terminals did register as own "session" with `/run/utmp` and
displayed the messages to the user in the terminal. As consequence, people
having a lot of terminals running got the message in every single terminal.
This was so annyoing, that several GUI don't support this anymore.
At the same time, desktop users without open terminal did never got this
informations.
With `systemd-logind` replacing `utmp`, this hack does not work anymore at all.
So this service provides a new, generic solution which should work for all
users.

## Applications and services

### wall-broadcaster service

The wall-broadcaster.service runs `wall-broadcaster`. This utility will open a PTY and registers itself as new session with this TTY on `systemd-logind`.
It will watch for messages on that TTY and tries to parse them. Messages send by `wall` or `write` will be splitted in a summary and body and cleaned up, else the message will be forwarded as "body".

The information are then send via the org.opensuse.WallBroadcast dbus interface.

### wall-bcst-gateway

This application will register to the system dbus and the user session dbus of the Desktop Environment and convert messages coming from org.opensuse.WallBroadcast to Freedesktop notifications. It should be run by the user.

### wall-bcst-watcher

This is a small utility, which listens to org.opensuse.WallBroadcast messages prints them on stdout.

## DBUS Protocol

The notification components are very similar to the
[Freedesktop Notifications Specification](https://specifications.freedesktop.org/notification/latest-single/) to make it easy to convert it to the desktop notification system.

| Component | Type | Description |
| --------- | ---- | ----------- |
| Application Name | String(s)| Optional name of the application sending the notification |
| Summary | String(s) | Single line overview |
| Body |String(s) | Multi-line text |
| Urgency |Byte(y) | 0=Low, 1=Normal, 2=Critical |
| Sender |String(s) | Optional name of the sender |

* Path=/org/opensuse/WallBroadcast
* Interface=org.opensuse.WallBroadcast

## Enable and Test
Reload systemd, enable the service, and start it.

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now wall-broadcaster.service
```

### Testing
* Open a new terminal and monitor the D-Bus signals using `dbus-monitor`:
  ```sh
  sudo dbus-monitor --system "sender='org.opensuse.WallBroadcast'"
  ```
* In another terminal, trigger a wall message:
  ```sh
  echo "System maintenance in 5 minutes!" | wall
  ```
* The `dbus-monitor` should print a `MessageReceived` signal with the parsed fields (Message, Sender, Priority, Receiver).
