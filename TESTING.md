devise and implement a test method as follows:

- build and flash the platformio project.
- write python test harnedss to test the mqtt broker at picomqtt.local:1883
- verify the broker connects
- wait  until the settings values are published under preferences/testns/.. (up to 10 secs)
- overwrite values except reset and reboot with random values of appropriate type.
- reboot the embedded system by pushing the value 1 to preferences/testns/reboot
- again wait  until the settings values are published under preferences/testns/.. (up to 10 secs)
- verify the new values are reported
- then trigger the preferences/testns/reset topic by a pushing the value 1
- reboot the embedded system by pushing the value 1 to preferences/testns/reboot
- wait  until the settings values are published under preferences/testns/.. (up to 10 secs)
- verify the default values are reported
- use the workspace .venv for all Python packages

