# INSTRUCTIONS

## Clone the git repo

```
git clone https://github.com/wizzardx/rrplayer8-deb10-amd64.git
```

# Compiling and testing the RR player

```
./test.py
```

If thisss succeeds, then yould have the RR player running in a Vagrant VM (including audio playback) as well as a built deb file in the current directory, eg: `rrplayer8_8.7.0-deb10_amd64.deb `

If this fails, then you can usually find the exact cause of the problem on the command-line.

The most critical config (eg things like version numbers or deb mirrors) can be found within `project_settings.yaml`

Almost all the logic can be  found within the `test.py script itself.

If you get stuck then you can find my contact details over here:

https://davidpurdy.ar-ciel.org/
