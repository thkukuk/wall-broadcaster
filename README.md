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

## Enable and Test
Reload systemd, enable the service, and start it.

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now wall-broadcaster.service
```

### Testing
* Open a new terminal and monitor the D-Bus signals using `dbus-monitor`:
  ```sh
  sudo dbus-monitor --system "sender='org.opensuse.WallBroadcaster'"
  ```
* In another terminal, trigger a wall message:
  ```sh
  echo "System maintenance in 5 minutes!" | wall
  ```
* The `dbus-monitor` should print a `MessageReceived` signal with the parsed fields (Message, Sender, Priority, Receiver).
